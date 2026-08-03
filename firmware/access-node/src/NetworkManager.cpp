#include "NetworkManager.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <Ethernet.h>
#include "config.h"

NetworkManager Network;

namespace {
const char *NVS_NAMESPACE = "accesscfg";
Preferences prefs;
WiFiClient wifiClient;
EthernetClient ethClient;
constexpr uint32_t ETH_REQUEST_TIMEOUT_MS = 3000;

String urlDecode(const String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < s.length()) {
      char hex[3] = {s[i + 1], s[i + 2], 0};
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

// Reads one "key=value" out of an application/x-www-form-urlencoded body —
// good enough for our own fixed, known set of field names, not a general
// parser (no repeated keys, no arrays).
String formValue(const String &body, const char *key) {
  String needle = String(key) + "=";
  int start = body.indexOf(needle);
  if (start < 0) return String();
  start += needle.length();
  int end = body.indexOf('&', start);
  String raw = (end < 0) ? body.substring(start) : body.substring(start, end);
  return urlDecode(raw);
}
}  // namespace

void NetworkManager::loadConfig() {
  prefs.begin(NVS_NAMESPACE, true);
  _cfg.connMode = (ConnMode)prefs.getUChar("mode", (uint8_t)ConnMode::WIFI);
  _cfg.backendUrl = prefs.getString("backend_url", "");
  _cfg.apiKey = prefs.getString("api_key", "");
  _cfg.syncIntervalS = prefs.getUInt("sync_s", DEFAULT_SYNC_INTERVAL_S);
  _cfg.relayPulseMs = prefs.getUInt("pulse_ms", DEFAULT_RELAY_PULSE_MS);
  _cfg.tz = prefs.getString("tz", DEFAULT_TZ);
  _cfg.doorLabel = prefs.getString("label", "");
  prefs.end();
}

void NetworkManager::saveConfig() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUChar("mode", (uint8_t)_cfg.connMode);
  prefs.putString("backend_url", _cfg.backendUrl);
  prefs.putString("api_key", _cfg.apiKey);
  prefs.putUInt("sync_s", _cfg.syncIntervalS);
  prefs.putUInt("pulse_ms", _cfg.relayPulseMs);
  prefs.putString("tz", _cfg.tz);
  prefs.putString("label", _cfg.doorLabel);
  prefs.end();
}

// Plain text field instead of a <select> — WiFiManagerParameter doesn't
// support real dropdowns, and hand-rolling one means poking at the
// library's internal web server to read the posted value back out.
// Called once from begin(), after loadConfig() — see the class comment on
// _wm for why these can't be re-added on every portal open.
void NetworkManager::setupParams() {
  refreshParamBuffers();

  _pMode = new WiFiManagerParameter("conn_mode", "Modo: 'wifi' o 'eth' (modulo W5500)", _bufMode, sizeof(_bufMode));
  _pBackend = new WiFiManagerParameter("backend_url", "URL del backend (http://host:8010)", _bufBackend, sizeof(_bufBackend));
  _pApiKey = new WiFiManagerParameter("api_key", "API key de esta puerta", _bufApiKey, sizeof(_bufApiKey));
  _pSync = new WiFiManagerParameter("sync_s", "Intervalo de sync (segundos)", _bufSync, sizeof(_bufSync));
  _pPulse = new WiFiManagerParameter("pulse_ms", "Duracion pulso rele (ms)", _bufPulse, sizeof(_bufPulse));
  _pTz = new WiFiManagerParameter("tz", "TZ POSIX (ver nayarsystems/posix_tz_db)", _bufTz, sizeof(_bufTz));
  _pLabel = new WiFiManagerParameter("label", "Etiqueta del nodo (opcional)", _bufLabel, sizeof(_bufLabel));

  _wm.addParameter(_pMode);
  _wm.addParameter(_pBackend);
  _wm.addParameter(_pApiKey);
  _wm.addParameter(_pSync);
  _wm.addParameter(_pPulse);
  _wm.addParameter(_pTz);
  _wm.addParameter(_pLabel);
}

// Re-fills the buffers WiFiManagerParameter points at (from current _cfg,
// falling back to the compiled defaults for anything unset) and pushes them
// into the already-added parameters via setValue() — so whichever portal
// gets opened next (AP or LAN) shows what's actually configured right now,
// not whatever was on screen the first time setupParams() ran.
void NetworkManager::refreshParamBuffers() {
  strncpy(_bufMode, _cfg.connMode == ConnMode::ETHERNET ? "eth" : "wifi", sizeof(_bufMode));
  strncpy(_bufBackend, _cfg.backendUrl.length() ? _cfg.backendUrl.c_str() : DEFAULT_BACKEND_URL, sizeof(_bufBackend));
  strncpy(_bufApiKey, _cfg.apiKey.c_str(), sizeof(_bufApiKey));
  snprintf(_bufSync, sizeof(_bufSync), "%u", _cfg.syncIntervalS ? _cfg.syncIntervalS : DEFAULT_SYNC_INTERVAL_S);
  snprintf(_bufPulse, sizeof(_bufPulse), "%u", _cfg.relayPulseMs ? _cfg.relayPulseMs : DEFAULT_RELAY_PULSE_MS);
  strncpy(_bufTz, _cfg.tz.length() ? _cfg.tz.c_str() : DEFAULT_TZ, sizeof(_bufTz));
  strncpy(_bufLabel, _cfg.doorLabel.c_str(), sizeof(_bufLabel));

  if (_pMode) {
    _pMode->setValue(_bufMode, sizeof(_bufMode));
    _pBackend->setValue(_bufBackend, sizeof(_bufBackend));
    _pApiKey->setValue(_bufApiKey, sizeof(_bufApiKey));
    _pSync->setValue(_bufSync, sizeof(_bufSync));
    _pPulse->setValue(_bufPulse, sizeof(_bufPulse));
    _pTz->setValue(_bufTz, sizeof(_bufTz));
    _pLabel->setValue(_bufLabel, sizeof(_bufLabel));
  }
}

void NetworkManager::applyParamsToConfig() {
  String modeValue = String(_pMode->getValue());
  modeValue.toLowerCase();
  _cfg.connMode = (modeValue == "eth") ? ConnMode::ETHERNET : ConnMode::WIFI;
  _cfg.backendUrl = String(_pBackend->getValue());
  _cfg.apiKey = String(_pApiKey->getValue());
  _cfg.syncIntervalS = atoi(_pSync->getValue());
  _cfg.relayPulseMs = atoi(_pPulse->getValue());
  _cfg.tz = String(_pTz->getValue());
  _cfg.doorLabel = String(_pLabel->getValue());
  if (_cfg.syncIntervalS < 10) _cfg.syncIntervalS = DEFAULT_SYNC_INTERVAL_S;
  if (_cfg.relayPulseMs < 100) _cfg.relayPulseMs = DEFAULT_RELAY_PULSE_MS;
  if (_cfg.tz.length() == 0) _cfg.tz = DEFAULT_TZ;
}

// Fired by WiFiManager from inside the web server's request handler when
// the form is submitted — whether that's the LAN portal's "Setup" page
// (params only, no WiFi credentials touched — triggers setSaveParamsCallback)
// or its "Configure WiFi" page (SSID/password + our params together, same
// as the AP flow — triggers setSaveConfigCallback). Both are wired to this
// same handler so either path saves correctly. Restarting here directly
// would cut off the "saved" response mid-flight, so this only applies and
// persists the config, then arms a short delayed restart that loop() acts
// on once the response has had time to reach the browser.
void NetworkManager::onParamsSaved() {
  applyParamsToConfig();
  saveConfig();
  Serial.println("[net] Config saved via LAN web portal. Rebooting shortly...");
  _pendingRestart = true;
  _pendingRestartAtMs = millis() + 1500;
}

void NetworkManager::runConfigPortal() {
  refreshParamBuffers();

  _wm.setConfigPortalTimeout(0);  // wait indefinitely for the installer to configure it

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);  // solid on = waiting for configuration

  Serial.println("[net] Config portal starting.");
  Serial.println("[net] Connect to WiFi \"" AP_PORTAL_NAME "\" and open http://192.168.4.1 to configure this node.");

  // In WiFi mode this also collects and saves the STA SSID/password (via
  // WiFiManager's normal flow) which the ESP32 WiFi stack then persists on
  // its own. In Ethernet mode those credentials are simply left unused.
  _wm.startConfigPortal(AP_PORTAL_NAME);

  applyParamsToConfig();
  saveConfig();
  Serial.println("[net] Config saved. Rebooting...");
  delay(500);
  ESP.restart();
}

// Non-blocking counterpart to runConfigPortal(): once the node already has
// a WiFi (STA) connection, this starts the same WiFiManager web server —
// same "Configure WiFi" + custom-params pages — bound to the node's normal
// LAN IP instead of bringing up an AP. No button, no reboot to reach it;
// changes made here are picked up via onParamsSaved() above.
void NetworkManager::startWebConfigPortal() {
  if (!ENABLE_LAN_CONFIG_PORTAL) return;

  refreshParamBuffers();
  _wm.setSaveConfigCallback([this]() { onParamsSaved(); });
  _wm.setSaveParamsCallback([this]() { onParamsSaved(); });
  _wm.startWebPortal();
  _webPortalActive = true;
  Serial.printf("[net] LAN config portal available at http://%s/ (same form as the AP portal)\n",
                WiFi.localIP().toString().c_str());
}

// Same 7 fields as the WiFiManager form (minus WiFi SSID/password, which
// don't apply in Ethernet mode), rendered by hand since there's no HTML
// templating available here. Values come from the _bufXxx buffers that
// refreshParamBuffers() already keeps in sync with _cfg for the WiFi path
// — reused as-is rather than duplicating that logic.
String NetworkManager::ethConfigFormHtml() const {
  String html;
  html.reserve(1200);
  html += F("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>AccessNode config</title></head><body>"
             "<h3>Configuracion del nodo (Ethernet)</h3>"
             "<form method=\"POST\" action=\"/\">"
             "Modo ('wifi' o 'eth'): <input name=\"conn_mode\" value=\"");
  html += _bufMode;
  html += F("\"><br>URL del backend: <input name=\"backend_url\" size=\"40\" value=\"");
  html += _bufBackend;
  html += F("\"><br>API key de esta puerta: <input name=\"api_key\" size=\"40\" value=\"");
  html += _bufApiKey;
  html += F("\"><br>Intervalo de sync (s): <input name=\"sync_s\" value=\"");
  html += _bufSync;
  html += F("\"><br>Duracion pulso rele (ms): <input name=\"pulse_ms\" value=\"");
  html += _bufPulse;
  html += F("\"><br>TZ POSIX: <input name=\"tz\" size=\"30\" value=\"");
  html += _bufTz;
  html += F("\"><br>Etiqueta del nodo: <input name=\"label\" value=\"");
  html += _bufLabel;
  html += F("\"><br><br><button type=\"submit\">Guardar</button></form>"
             "<p>Cambiar el modo a 'wifi' aqui solo guarda esa preferencia — "
             "para meter el SSID/contrasena de la red sigue haciendo falta "
             "el boton de config + el AP.</p></body></html>");
  return html;
}

void NetworkManager::applyEthFormBody(const String &body) {
  String modeValue = formValue(body, "conn_mode");
  modeValue.toLowerCase();
  _cfg.connMode = (modeValue == "wifi") ? ConnMode::WIFI : ConnMode::ETHERNET;
  _cfg.backendUrl = formValue(body, "backend_url");
  _cfg.apiKey = formValue(body, "api_key");
  _cfg.syncIntervalS = (uint32_t)formValue(body, "sync_s").toInt();
  _cfg.relayPulseMs = (uint32_t)formValue(body, "pulse_ms").toInt();
  _cfg.tz = formValue(body, "tz");
  _cfg.doorLabel = formValue(body, "label");
  if (_cfg.syncIntervalS < 10) _cfg.syncIntervalS = DEFAULT_SYNC_INTERVAL_S;
  if (_cfg.relayPulseMs < 100) _cfg.relayPulseMs = DEFAULT_RELAY_PULSE_MS;
  if (_cfg.tz.length() == 0) _cfg.tz = DEFAULT_TZ;
}

// Starts the Ethernet-side counterpart to startWebConfigPortal() — see the
// class-level comment for why this can't just be WiFiManager again.
void NetworkManager::startEthConfigServer() {
  if (!ENABLE_LAN_CONFIG_PORTAL) return;
  _ethServer.begin();
  _ethServerActive = true;
  Serial.print("[net] LAN config portal available at http://");
  Serial.println(Ethernet.localIP());
}

// One request in, one response out, connection closed — no keep-alive, no
// chunked bodies, no concurrent clients. Reads block for up to
// ETH_REQUEST_TIMEOUT_MS per stage (mirrors SimpleHttp's style/timeouts),
// so a slow or half-open client can briefly stall the main loop; acceptable
// here since this only runs when someone is deliberately on the config
// page, on the LAN, same class of tradeoff as the rest of this firmware's
// synchronous I/O.
void NetworkManager::handleEthClient(EthernetClient &client) {
  client.setTimeout(ETH_REQUEST_TIMEOUT_MS);

  String requestLine = client.readStringUntil('\n');
  requestLine.trim();
  bool isPost = requestLine.startsWith("POST");

  int contentLength = 0;
  String line;
  do {
    line = client.readStringUntil('\n');
    line.trim();
    String lower = line;
    lower.toLowerCase();
    if (lower.startsWith("content-length:")) {
      contentLength = line.substring(line.indexOf(':') + 1).toInt();
    }
  } while (line.length() > 0 && client.connected());

  String body;
  if (isPost && contentLength > 0) {
    body.reserve(contentLength);
    uint32_t start = millis();
    while ((int)body.length() < contentLength && millis() - start < ETH_REQUEST_TIMEOUT_MS) {
      while (client.available() && (int)body.length() < contentLength) {
        body += (char)client.read();
      }
      if (!client.connected() && !client.available()) break;
    }
  }

  if (isPost) {
    applyEthFormBody(body);
    saveConfig();
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html; charset=utf-8"));
    client.println(F("Connection: close"));
    client.println();
    client.println(F("<html><body><h3>Guardado. Reiniciando...</h3></body></html>"));
    client.flush();
    delay(50);
    client.stop();
    Serial.println("[net] Config saved via Ethernet LAN portal. Rebooting shortly...");
    _pendingRestart = true;
    _pendingRestartAtMs = millis() + 1500;
    return;
  }

  refreshParamBuffers();
  String html = ethConfigFormHtml();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html; charset=utf-8"));
  client.print(F("Content-Length: "));
  client.println(html.length());
  client.println(F("Connection: close"));
  client.println();
  client.print(html);
  client.flush();
  delay(50);
  client.stop();
}

void NetworkManager::serviceEthConfigServer() {
  EthernetClient client = _ethServer.available();
  if (!client) return;
  handleEthClient(client);
}

void NetworkManager::connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();  // uses credentials the ESP32 WiFi stack already persisted
  Serial.print("[net] Connecting to WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[net] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[net] WiFi connect timed out — will keep retrying in the background.");
  }
}

void NetworkManager::connectEthernet() {
  uint8_t mac[6];
  WiFi.macAddress(mac);  // read the factory MAC before switching the radio off
  WiFi.mode(WIFI_OFF);
  mac[0] &= 0xFE;  // clear the multicast bit — keep it a valid unicast MAC

  SPI.begin(PIN_ETH_SCK, PIN_ETH_MISO, PIN_ETH_MOSI, PIN_ETH_CS);
  Ethernet.init(PIN_ETH_CS);

  pinMode(PIN_ETH_RST, OUTPUT);
  digitalWrite(PIN_ETH_RST, LOW);
  delay(50);
  digitalWrite(PIN_ETH_RST, HIGH);
  delay(300);

  Serial.println("[net] Starting Ethernet (DHCP)...");
  if (Ethernet.begin(mac, 8000) == 0) {
    Serial.println("[net] Ethernet DHCP failed — check the cable/link.");
  } else {
    Serial.print("[net] Ethernet connected, IP: ");
    Serial.println(Ethernet.localIP());
  }
}

void NetworkManager::begin() {
  pinMode(PIN_CONFIG_BUTTON, INPUT_PULLUP);
  loadConfig();
  setupParams();

  bool forcePortal = (digitalRead(PIN_CONFIG_BUTTON) == LOW);
  if (forcePortal) {
    Serial.println("[net] Config button held at boot — hold it to confirm reconfiguration...");
    uint32_t start = millis();
    while (digitalRead(PIN_CONFIG_BUTTON) == LOW && millis() - start < CONFIG_BUTTON_HOLD_MS) delay(10);
    forcePortal = (millis() - start >= CONFIG_BUTTON_HOLD_MS);
  }

  if (!_cfg.isValid() || forcePortal) {
    runConfigPortal();  // blocks, then reboots — never returns normally
  }

  if (_cfg.connMode == ConnMode::ETHERNET) {
    connectEthernet();
    if (Ethernet.linkStatus() != LinkOFF) startEthConfigServer();
  } else {
    connectWifi();
    if (WiFi.status() == WL_CONNECTED) startWebConfigPortal();
  }
}

void NetworkManager::loop() {
  if (_pendingRestart && millis() >= _pendingRestartAtMs) {
    ESP.restart();
  }

  if (_webPortalActive) _wm.process();
  if (_ethServerActive) serviceEthConfigServer();

  if (_cfg.connMode == ConnMode::ETHERNET) {
    Ethernet.maintain();
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastAttempt = 0;
    if (millis() - lastAttempt > 10000) {
      lastAttempt = millis();
      WiFi.reconnect();
    }
  }
}

Client &NetworkManager::client() {
  if (_cfg.connMode == ConnMode::ETHERNET) return ethClient;
  return wifiClient;
}

bool NetworkManager::isConnected() {
  if (_cfg.connMode == ConnMode::ETHERNET) {
    return Ethernet.linkStatus() != LinkOFF;
  }
  return WiFi.status() == WL_CONNECTED;
}
