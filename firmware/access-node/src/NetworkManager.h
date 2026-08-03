#pragma once
#include <Arduino.h>
#include <Client.h>
#include <WiFiManager.h>
#include <Ethernet.h>

enum class ConnMode : uint8_t { WIFI = 0, ETHERNET = 1 };

// arduino-libraries/Ethernet's EthernetServer declares begin() with no
// arguments (the port is fixed at construction) — one signature short of
// the pure virtual Server::begin(uint16_t port=0) the ESP32 Arduino core
// requires, so plain EthernetServer is left abstract on this platform and
// can't be instantiated. This overrides the exact signature the core
// wants and forwards to the real begin(), ignoring the (redundant,
// already-known) port argument.
class ConfigEthernetServer : public EthernetServer {
 public:
  explicit ConfigEthernetServer(uint16_t port) : EthernetServer(port) {}
  void begin(uint16_t port = 0) override {
    (void)port;
    EthernetServer::begin();
  }
};

struct RuntimeConfig {
  ConnMode connMode = ConnMode::WIFI;
  String backendUrl;
  String apiKey;
  uint32_t syncIntervalS = 0;
  uint32_t relayPulseMs = 0;
  String tz;
  String doorLabel;

  // Wiring — configurable from the same config portal as everything else
  // above (see NetworkManager's params), persisted in NVS. Defaults come
  // from config.h and only matter before the portal has ever been saved.
  int8_t wiegandD0Pin = -1;
  int8_t wiegandD1Pin = -1;
  int8_t relayPin = -1;
  bool relayActiveHigh = true;
  int8_t doorSensorPin = -1;  // -1 = disabled, same convention as before
  bool doorSensorClosedHigh = false;

  bool isValid() const { return backendUrl.length() > 0 && apiKey.length() > 0; }
};

// Owns node configuration (persisted in NVS) and whichever transport
// (WiFi STA or a W5500 Ethernet module) the node was configured to use.
// First boot — or holding the config button — launches a blocking
// WiFiManager captive portal (its own AP) to collect everything, then
// reboots into normal operation. Once connected, the exact same set of
// fields is also reachable on the LAN at the node's own IP with no AP and
// no button: startWebConfigPortal() over WiFi (WiFiManager's own
// non-blocking web portal), startEthConfigServer() over Ethernet (a small
// hand-rolled HTTP server — WiFiManager can't serve over the W5500, see
// the comment on that method).
class NetworkManager {
public:
  void begin();
  void loop();

  const RuntimeConfig &config() const { return _cfg; }
  Client &client();
  bool isConnected();

private:
  RuntimeConfig _cfg;

  // Both the blocking AP portal and the non-blocking LAN web portal share
  // this single WiFiManager instance and its parameters — the params are
  // added to it exactly once (setupParams(), called from begin()).
  // WiFiManager doesn't dedupe addParameter() calls, so adding them again
  // on every portal open would just render each field twice.
  WiFiManager _wm;

  char _bufMode[8];
  char _bufBackend[128];
  char _bufApiKey[80];
  char _bufSync[8];
  char _bufPulse[8];
  char _bufTz[64];
  char _bufLabel[40];
  char _bufWgD0[4];
  char _bufWgD1[4];
  char _bufRelayPin[4];
  char _bufRelayAh[4];
  char _bufSensorPin[4];
  char _bufSensorCh[4];
  WiFiManagerParameter *_pMode = nullptr;
  WiFiManagerParameter *_pBackend = nullptr;
  WiFiManagerParameter *_pApiKey = nullptr;
  WiFiManagerParameter *_pSync = nullptr;
  WiFiManagerParameter *_pPulse = nullptr;
  WiFiManagerParameter *_pTz = nullptr;
  WiFiManagerParameter *_pLabel = nullptr;
  WiFiManagerParameter *_pWgD0 = nullptr;
  WiFiManagerParameter *_pWgD1 = nullptr;
  WiFiManagerParameter *_pRelayPin = nullptr;
  WiFiManagerParameter *_pRelayAh = nullptr;
  WiFiManagerParameter *_pSensorPin = nullptr;
  WiFiManagerParameter *_pSensorCh = nullptr;

  bool _webPortalActive = false;
  bool _pendingRestart = false;
  uint32_t _pendingRestartAtMs = 0;

  // Minimal hand-rolled HTTP server for the LAN config page over Ethernet
  // — arduino-libraries/Ethernet implements its own TCP/IP stack against
  // the W5500 over SPI, entirely separate from the WiFi/lwIP stack
  // WiFiManager's web portal is built on, so that portal simply cannot be
  // reached over this interface.
  ConfigEthernetServer _ethServer{80};
  bool _ethServerActive = false;

  void loadConfig();
  void saveConfig();
  void setupParams();
  void refreshParamBuffers();
  void applyParamsToConfig();
  void onParamsSaved();
  void runConfigPortal();
  void startWebConfigPortal();
  void startEthConfigServer();
  void serviceEthConfigServer();
  void handleEthClient(EthernetClient &client);
  String ethConfigFormHtml() const;
  void applyEthFormBody(const String &body);
  void connectWifi();
  void connectEthernet();
};

extern NetworkManager Network;
