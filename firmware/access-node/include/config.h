#pragma once

// Pin assignments for a generic ESP32 DevKit (WROOM-32). Adjust to taste —
// avoid strapping pins (0, 2, 5, 12, 15) and the UART0 pins (1, 3) for
// anything that must be stable at boot.

// ---- Wiegand reader (D0/D1, idle-high, pulses LOW) ----
#define PIN_WIEGAND_D0 4
#define PIN_WIEGAND_D1 16

// Optional feedback lines toward the reader — most readers work fine
// without them. Set to -1 to leave disconnected/unused.
#define PIN_READER_LED -1     // usually active LOW
#define PIN_READER_BUZZER -1  // usually active LOW pulse = beep

// ---- Relay driving the gate controller's START/PED dry contact input ----
#define PIN_RELAY 27
#define RELAY_ACTIVE_HIGH true  // flip to false if your relay module triggers on LOW

// ---- Door position sensor (reed switch on the door/gate leaf) — optional.
//      Set PIN_DOOR_SENSOR to -1 (default) to leave this feature off
//      entirely; the node then never touches this pin or the /sensor
//      endpoint. Wire the switch between the pin and GND and leave the
//      internal pull-up doing the rest (no external resistor needed). Most
//      door reed switches are wired so the magnet (on the moving leaf)
//      keeps the contacts closed when the door is shut — that shorts the
//      pin to GND, i.e. LOW = closed, HIGH = open. Flip the flag below if
//      yours is wired the other way round. ----
#define PIN_DOOR_SENSOR -1
#define DOOR_SENSOR_CLOSED_HIGH false  // false (default/typical): LOW = closed, HIGH = open. true: the reverse.
#define DOOR_SENSOR_DEBOUNCE_MS 100    // pin must read the same value for this long before a transition is accepted — filters contact bounce/EMI, not meant to filter out a real slow-swinging door
// How long after a granted access (card or manual trigger actually fired
// the relay) the door is allowed to open before it's no longer considered
// "expected" — tune this to how slow your specific gate/door mechanism is.
// An opening detected outside this window is reported as forced.
#define DOOR_OPEN_GRACE_MS 15000

// ---- Hold at boot to force the config portal even if valid config exists ----
#define PIN_CONFIG_BUTTON 0  // BOOT button on most ESP32 DevKit boards
#define CONFIG_BUTTON_HOLD_MS 3000

// ---- LAN config portal — once the node is connected over WiFi, the exact
//      same form the AP portal shows (backend URL, API key, sync/pulse/TZ,
//      WiFi credentials) is also reachable at the node's own LAN IP
//      (printed to Serial at boot), no button or AP needed. Same
//      plaintext-HTTP posture as the rest of this firmware (see README).
//      Set to false to disable it and require the button+AP flow for every
//      config change instead. WiFi-only: WiFiManager is built around the
//      WiFi stack, so this stays off in Ethernet mode regardless.
#define ENABLE_LAN_CONFIG_PORTAL true

// ---- Status LED (blinks to show WiFi/ETH + backend sync health) ----
#define PIN_STATUS_LED 2

// ---- W5500 Ethernet module over SPI (VSPI defaults), used when the
//      portal's connection mode is set to "ethernet" ----
#define PIN_ETH_CS 5
#define PIN_ETH_RST 33
#define PIN_ETH_SCK 18
#define PIN_ETH_MISO 19
#define PIN_ETH_MOSI 23

// ---- Defaults offered on the first-boot config portal ----
#define DEFAULT_BACKEND_URL "http://192.168.1.10:8010"
#define DEFAULT_SYNC_INTERVAL_S 120
#define DEFAULT_RELAY_PULSE_MS 600
// POSIX TZ string used to compute local day-of-week/time-of-day for
// schedules — default is Europe/Madrid. See https://github.com/nayarsystems/posix_tz_db
#define DEFAULT_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

#define AP_PORTAL_NAME "AccessNode-Setup"

#define MAX_CACHED_CREDENTIALS 500
#define MAX_QUEUED_LOG_EVENTS 200
