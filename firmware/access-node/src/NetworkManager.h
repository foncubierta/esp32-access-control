#pragma once
#include <Arduino.h>
#include <Client.h>
#include <WiFiManager.h>
#include <Ethernet.h>

enum class ConnMode : uint8_t { WIFI = 0, ETHERNET = 1 };

struct RuntimeConfig {
  ConnMode connMode = ConnMode::WIFI;
  String backendUrl;
  String apiKey;
  uint32_t syncIntervalS = 0;
  uint32_t relayPulseMs = 0;
  String tz;
  String doorLabel;

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
  WiFiManagerParameter *_pMode = nullptr;
  WiFiManagerParameter *_pBackend = nullptr;
  WiFiManagerParameter *_pApiKey = nullptr;
  WiFiManagerParameter *_pSync = nullptr;
  WiFiManagerParameter *_pPulse = nullptr;
  WiFiManagerParameter *_pTz = nullptr;
  WiFiManagerParameter *_pLabel = nullptr;

  bool _webPortalActive = false;
  bool _pendingRestart = false;
  uint32_t _pendingRestartAtMs = 0;

  // Minimal hand-rolled HTTP server for the LAN config page over Ethernet
  // — arduino-libraries/Ethernet implements its own TCP/IP stack against
  // the W5500 over SPI, entirely separate from the WiFi/lwIP stack
  // WiFiManager's web portal is built on, so that portal simply cannot be
  // reached over this interface.
  EthernetServer _ethServer{80};
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
