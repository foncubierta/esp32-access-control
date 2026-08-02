#pragma once
#include <Arduino.h>
#include <Client.h>

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
// First boot — or holding the config button — launches a WiFiManager
// captive portal to collect everything, then reboots into normal operation.
class NetworkManager {
public:
  void begin();
  void loop();

  const RuntimeConfig &config() const { return _cfg; }
  Client &client();
  bool isConnected();

private:
  RuntimeConfig _cfg;

  void loadConfig();
  void saveConfig();
  void runConfigPortal();
  void connectWifi();
  void connectEthernet();
};

extern NetworkManager Network;
