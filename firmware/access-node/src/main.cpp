#include <Arduino.h>
#include <time.h>
#include "AccessController.h"
#include "BackendApi.h"
#include "CredentialHash.h"
#include "EventQueue.h"
#include "NetworkManager.h"
#include "RelayController.h"
#include "TimeUtil.h"
#include "WiegandReader.h"
#include "config.h"

namespace {
uint32_t lastSyncMs = 0;
uint32_t lastLogFlushMs = 0;
uint32_t lastHeartbeatMs = 0;
constexpr uint32_t LOG_FLUSH_INTERVAL_MS = 15000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 60000;

String nowIso8601() {
  time_t t = time(nullptr);
  struct tm utc;
  gmtime_r(&t, &utc);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return String(buf);
}

void handleCardScan() {
  String rawValue;
  uint8_t bitCount;
  uint16_t facility;
  uint32_t card;
  Wiegand.getFrame(rawValue, bitCount, facility, card);
  if (rawValue.length() == 0) return;

  String hash = sha256HexOfCredential(rawValue);
  AccessDecision decision = Access.evaluate(hash);
  bool granted = decision.result == AccessResult::GRANTED;

  Serial.printf("[card] %u bits raw=%s hash=%s -> %s", bitCount, rawValue.c_str(), hash.c_str(),
                granted ? "GRANTED" : "DENIED");
  if (bitCount == 26) Serial.printf(" (26-bit FC=%u CN=%lu)", facility, (unsigned long)card);
  if (decision.reason) Serial.printf(" reason=%s", decision.reason);
  Serial.println();
  // ^ This is also how you enroll a new card: badge it here, copy the raw
  // value printed above, and paste it into the credential's "Value" field
  // in the admin panel (Credenciales > Nueva credencial).

  LogEvent ev;
  ev.valueHash = hash;
  ev.credentialId = decision.credentialId;
  ev.result = granted ? "granted" : "denied";
  ev.reason = decision.reason ? String(decision.reason) : String();
  ev.eventTimeIso = nowIso8601();
  PendingLogs.push(ev);

  if (granted) {
    Relay.pulse(Network.config().relayPulseMs);
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[main] ESP32 Access Control node starting");

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  Relay.begin(PIN_RELAY, RELAY_ACTIVE_HIGH);
  Wiegand.begin(PIN_WIEGAND_D0, PIN_WIEGAND_D1);

  Network.begin();  // runs the config portal (and reboots) on first boot

  if (Network.isConnected()) {
    if (!TimeUtil::syncNtp(Network.config().tz)) {
      Serial.println("[main] NTP sync timed out — schedules may be evaluated against a wrong clock until it lands.");
    }
    BackendApi::sync();
    lastSyncMs = millis();
  } else {
    Serial.println("[main] Not connected yet — will keep the cached credential list (empty on first boot) until it comes up.");
  }

  Serial.println("[main] Ready.");
}

void loop() {
  Network.loop();

  uint32_t now = millis();
  uint32_t syncIntervalMs = Network.config().syncIntervalS * 1000UL;

  if (Network.isConnected() && now - lastSyncMs >= syncIntervalMs) {
    lastSyncMs = now;
    BackendApi::sync();
  }

  if (Network.isConnected() && !PendingLogs.empty() && now - lastLogFlushMs >= LOG_FLUSH_INTERVAL_MS) {
    lastLogFlushMs = now;
    BackendApi::uploadLogs();
  }

  if (Network.isConnected() && now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    BackendApi::heartbeat();
  }

  if (Wiegand.available()) {
    handleCardScan();
  }
}
