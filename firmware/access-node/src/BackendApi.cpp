#include "BackendApi.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "AccessController.h"
#include "EventQueue.h"
#include "NetworkManager.h"
#include "SimpleHttp.h"
#include "TimeUtil.h"
#include "config.h"

namespace {
constexpr size_t LOG_BATCH_SIZE = 20;

bool parseHostPort(const String &url, String &host, uint16_t &port) {
  String rest = url;
  if (rest.startsWith("http://")) {
    rest = rest.substring(7);
  } else if (rest.startsWith("https://")) {
    Serial.println("[api] HTTPS backend URLs aren't supported by this minimal client — use http://");
    return false;
  }
  int slash = rest.indexOf('/');
  if (slash >= 0) rest = rest.substring(0, slash);
  int colon = rest.indexOf(':');
  if (colon >= 0) {
    host = rest.substring(0, colon);
    port = (uint16_t)rest.substring(colon + 1).toInt();
  } else {
    host = rest;
    port = 80;
  }
  return host.length() > 0;
}
}  // namespace

bool BackendApi::sync() {
  if (!Network.isConnected()) return false;

  String host;
  uint16_t port;
  if (!parseHostPort(Network.config().backendUrl, host, port)) return false;

  HttpResponse resp = SimpleHttp::request(Network.client(), host, port, "/api/node/sync", "GET", Network.config().apiKey);
  if (resp.statusCode != 200) {
    Serial.printf("[api] sync failed, HTTP %d\n", resp.statusCode);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp.body);
  if (err) {
    Serial.printf("[api] sync JSON parse error: %s\n", err.c_str());
    return false;
  }

  bool doorActive = doc["door_active"] | true;
  const char *doorMode = doc["door_mode"] | "auto";
  JsonArray creds = doc["credentials"].as<JsonArray>();

  static CachedCredential buffer[MAX_CACHED_CREDENTIALS];
  size_t count = 0;
  for (JsonObject c : creds) {
    if (count >= MAX_CACHED_CREDENTIALS) break;
    CachedCredential &item = buffer[count];
    item.credentialId = c["credential_id"] | 0;
    const char *valueHash = c["value_hash"] | "";
    const char *daysOfWeek = c["days_of_week"] | "";
    const char *timeStart = c["time_start"] | "";
    const char *timeEnd = c["time_end"] | "";
    const char *validFrom = c["valid_from"] | (const char *)nullptr;
    const char *validUntil = c["valid_until"] | (const char *)nullptr;
    item.valueHash = String(valueHash);
    item.daysOfWeek = String(daysOfWeek);
    item.timeStart = String(timeStart);
    item.timeEnd = String(timeEnd);
    item.validFrom = validFrom ? TimeUtil::parseIso8601Utc(String(validFrom)) : 0;
    item.validUntil = validUntil ? TimeUtil::parseIso8601Utc(String(validUntil)) : 0;
    count++;
  }

  Access.replaceCache(buffer, count);
  Access.setDoorActive(doorActive);
  Access.setMode(String(doorMode));
  Serial.printf("[api] sync OK — %u credential(s) cached, door_active=%d, mode=%s\n", (unsigned)count, doorActive,
                doorMode);
  return true;
}

bool BackendApi::syncMode() {
  if (!Network.isConnected()) return false;

  String host;
  uint16_t port;
  if (!parseHostPort(Network.config().backendUrl, host, port)) return false;

  HttpResponse resp = SimpleHttp::request(Network.client(), host, port, "/api/node/mode", "GET", Network.config().apiKey);
  if (resp.statusCode != 200) return false;

  JsonDocument doc;
  if (deserializeJson(doc, resp.body)) return false;

  Access.setDoorActive(doc["door_active"] | true);
  Access.setMode(String((const char *)(doc["door_mode"] | "auto")));
  return true;
}

bool BackendApi::uploadLogs() {
  if (PendingLogs.empty()) return true;
  if (!Network.isConnected()) return false;

  String host;
  uint16_t port;
  if (!parseHostPort(Network.config().backendUrl, host, port)) return false;

  LogEvent batch[LOG_BATCH_SIZE];
  size_t n = PendingLogs.peek(batch, LOG_BATCH_SIZE);

  JsonDocument doc;
  JsonArray entries = doc["entries"].to<JsonArray>();
  for (size_t i = 0; i < n; i++) {
    JsonObject e = entries.add<JsonObject>();
    if (batch[i].valueHash.length()) e["value_hash"] = batch[i].valueHash;
    if (batch[i].credentialId >= 0) e["credential_id"] = batch[i].credentialId;
    e["result"] = batch[i].result;
    if (batch[i].reason.length()) e["reason"] = batch[i].reason;
    if (batch[i].doorMode.length()) e["door_mode"] = batch[i].doorMode;
    e["event_time"] = batch[i].eventTimeIso;
  }

  String body;
  serializeJson(doc, body);

  HttpResponse resp = SimpleHttp::request(Network.client(), host, port, "/api/node/logs", "POST", Network.config().apiKey, body);
  if (resp.statusCode == 200) {
    PendingLogs.ackFront(n);
    Serial.printf("[api] uploaded %u log event(s), %u still queued\n", (unsigned)n, (unsigned)PendingLogs.size());
    return true;
  }
  Serial.printf("[api] log upload failed, HTTP %d — will retry next cycle\n", resp.statusCode);
  return false;
}

bool BackendApi::heartbeat() {
  if (!Network.isConnected()) return false;

  String host;
  uint16_t port;
  if (!parseHostPort(Network.config().backendUrl, host, port)) return false;

  HttpResponse resp = SimpleHttp::request(Network.client(), host, port, "/api/node/heartbeat", "POST", Network.config().apiKey, "{}");
  return resp.statusCode == 200;
}
