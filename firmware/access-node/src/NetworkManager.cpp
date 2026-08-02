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

void NetworkManager::runConfigPortal() {
  WiFiManager wm;

  char bufMode[8];
  strncpy(bufMode, _cfg.connMode == ConnMode::ETHERNET ? "eth" : "wifi", sizeof(bufMode));
  char bufBackend[128];
  strncpy(bufBackend, _cfg.backendUrl.length() ? _cfg.backendUrl.c_str() : DEFAULT_BACKEND_URL, sizeof(bufBackend));
  char bufApiKey[80];
  strncpy(bufApiKey, _cfg.apiKey.c_str(), sizeof(bufApiKey));
  char bufSync[8];
  snprintf(bufSync, sizeof(bufSync), "%u", _cfg.syncIntervalS ? _cfg.syncIntervalS : DEFAULT_SYNC_INTERVAL_S);
  char bufPulse[8];
  snprintf(bufPulse, sizeof(bufPulse), "%u", _cfg.relayPulseMs ? _cfg.relayPulseMs : DEFAULT_RELAY_PULSE_MS);
  char bufTz[64];
  strncpy(bufTz, _cfg.tz.length() ? _cfg.tz.c_str() : DEFAULT_TZ, sizeof(bufTz));
  char bufLabel[40];
  strncpy(bufLabel, _cfg.doorLabel.c_str(), sizeof(bufLabel));

  // Plain text field instead of a <select> — WiFiManagerParameter doesn't
  // support real dropdowns, and hand-rolling one means poking at the
  // library's internal web server to read the posted value back out.
  WiFiManagerParameter pMode("conn_mode", "Modo: 'wifi' o 'eth' (modulo W5500)", bufMode, sizeof(bufMode));
  WiFiManagerParameter pBackend("backend_url", "URL del backend (http://host:8010)", bufBackend, sizeof(bufBackend));
  WiFiManagerParameter pApiKey("api_key", "API key de esta puerta", bufApiKey, sizeof(bufApiKey));
  WiFiManagerParameter pSync("sync_s", "Intervalo de sync (segundos)", bufSync, sizeof(bufSync));
  WiFiManagerParameter pPulse("pulse_ms", "Duracion pulso rele (ms)", bufPulse, sizeof(bufPulse));
  WiFiManagerParameter pTz("tz", "TZ POSIX (ver nayarsystems/posix_tz_db)", bufTz, sizeof(bufTz));
  WiFiManagerParameter pLabel("label", "Etiqueta del nodo (opcional)", bufLabel, sizeof(bufLabel));

  wm.addParameter(&pMode);
  wm.addParameter(&pBackend);
  wm.addParameter(&pApiKey);
  wm.addParameter(&pSync);
  wm.addParameter(&pPulse);
  wm.addParameter(&pTz);
  wm.addParameter(&pLabel);

  wm.setConfigPortalTimeout(0);  // wait indefinitely for the installer to configure it

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);  // solid on = waiting for configuration

  Serial.println("[net] Config portal starting.");
  Serial.println("[net] Connect to WiFi \"" AP_PORTAL_NAME "\" and open http://192.168.4.1 to configure this node.");

  // In WiFi mode this also collects and saves the STA SSID/password (via
  // WiFiManager's normal flow) which the ESP32 WiFi stack then persists on
  // its own. In Ethernet mode those credentials are simply left unused.
  wm.startConfigPortal(AP_PORTAL_NAME);

  String modeValue = String(pMode.getValue());
  modeValue.toLowerCase();
  _cfg.connMode = (modeValue == "eth") ? ConnMode::ETHERNET : ConnMode::WIFI;
  _cfg.backendUrl = String(pBackend.getValue());
  _cfg.apiKey = String(pApiKey.getValue());
  _cfg.syncIntervalS = atoi(pSync.getValue());
  _cfg.relayPulseMs = atoi(pPulse.getValue());
  _cfg.tz = String(pTz.getValue());
  _cfg.doorLabel = String(pLabel.getValue());
  if (_cfg.syncIntervalS < 10) _cfg.syncIntervalS = DEFAULT_SYNC_INTERVAL_S;
  if (_cfg.relayPulseMs < 100) _cfg.relayPulseMs = DEFAULT_RELAY_PULSE_MS;
  if (_cfg.tz.length() == 0) _cfg.tz = DEFAULT_TZ;

  saveConfig();
  Serial.println("[net] Config saved. Rebooting...");
  delay(500);
  ESP.restart();
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
  } else {
    connectWifi();
  }
}

void NetworkManager::loop() {
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
