#include <Arduino.h>
#include <time.h>
#include "AccessController.h"
#include "BackendApi.h"
#include "CredentialHash.h"
#include "DoorSensor.h"
#include "EventQueue.h"
#include "NetworkManager.h"
#include "RelayController.h"
#include "TimeUtil.h"
#include "WiegandReader.h"
#include "config.h"

namespace {
uint32_t lastSyncMs = 0;
uint32_t lastModePollMs = 0;
uint32_t lastLogFlushMs = 0;
uint32_t lastHeartbeatMs = 0;
constexpr uint32_t MODE_POLL_INTERVAL_MS = 2000;
constexpr uint32_t LOG_FLUSH_INTERVAL_MS = 15000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 60000;

// Last time the relay actually fired because of a granted card scan or a
// granted manual "open now" — the door-sensor logic below treats an
// opening within DOOR_OPEN_GRACE_MS of this as expected, anything later
// (or with no prior grant at all, since millis() starts at 0) as forced.
uint32_t lastRelayGrantMs = 0;

bool sensorReportPending = false;
bool sensorReportOpenValue = false;
bool sensorReportForcedValue = false;
uint32_t lastSensorReportAttemptMs = 0;
constexpr uint32_t SENSOR_REPORT_RETRY_MS = 3000;

String nowIso8601() {
  time_t t = time(nullptr);
  struct tm utc;
  gmtime_r(&t, &utc);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return String(buf);
}

// The access decision (would this credential normally get through?) and the
// physical action (does the relay actually fire?) are deliberately separate
// — "auto" is the only mode where they're the same thing.
bool shouldPulseRelay(const String &mode, bool doorActive, AccessResult result) {
  if (!doorActive) return false;
  if (mode == "open") return true;      // force-open: everyone gets a pulse
  if (mode == "closed") return false;   // force-closed: no one does, even with valid access
  if (mode == "identify") return false; // identify-only: never actuate, just log who it was
  return result == AccessResult::GRANTED;  // "auto" (and any unrecognized value)
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
  String mode = Access.mode();
  bool pulseRelay = shouldPulseRelay(mode, Access.doorActive(), decision.result);

  Serial.printf("[card] %u bits raw=%s hash=%s eval=%s", bitCount, rawValue.c_str(), hash.c_str(),
                decision.result == AccessResult::GRANTED ? "GRANTED" : "DENIED");
  if (bitCount == 26) Serial.printf(" (26-bit FC=%u CN=%lu)", facility, (unsigned long)card);
  if (decision.reason) Serial.printf(" reason=%s", decision.reason);
  Serial.printf(" mode=%s relay=%s\n", mode.c_str(), pulseRelay ? "FIRED" : "no");
  // ^ Enrolling a new card is normally done from the admin panel's "Leer
  // tarjeta" button (Credenciales > Nueva credencial) — see the
  // enrollArmed block below. This raw value printed to Serial is the
  // fallback for when you don't have that handy.

  // Fire the relay before any network I/O — a slow or unreachable backend
  // must never delay the physical door action.
  if (pulseRelay) {
    Relay.pulse(Network.config().relayPulseMs);
    lastRelayGrantMs = millis();
  }

  LogEvent ev;
  ev.valueHash = hash;
  ev.credentialId = decision.credentialId;
  ev.result = (decision.result == AccessResult::GRANTED) ? "granted" : "denied";
  ev.reason = decision.reason ? String(decision.reason) : String();
  ev.doorMode = mode;
  ev.eventTimeIso = nowIso8601();
  PendingLogs.push(ev);

  // Best-effort immediate upload so the guard view sees it within a couple
  // of seconds instead of waiting for the next periodic flush — falls back
  // to that periodic flush if this fails (offline), so nothing is lost.
  if (Network.isConnected()) {
    BackendApi::uploadLogs();
  }

  // Doesn't affect the access decision or the relay at all — purely an
  // additional, best-effort side report for the admin's "Leer tarjeta"
  // button, only sent while a session is actually armed.
  if (Access.enrollArmed() && Network.isConnected()) {
    BackendApi::reportEnrollment(rawValue, bitCount);
  }
}

// Fires a single pulse whenever the guard clicks "abrir ahora" — detected
// as Access.triggerSeq() (refreshed by the mode/sync polls) changing since
// the last value we acted on. The very first read after boot only sets the
// baseline instead of firing, so a node coming back online doesn't replay
// whatever trigger happened while it was offline.
void checkManualTrigger() {
  static int32_t lastAcked = -1;
  int32_t seq = Access.triggerSeq();
  if (lastAcked == -1) {
    lastAcked = seq;
    return;
  }
  if (seq == lastAcked) return;
  lastAcked = seq;

  bool doorActive = Access.doorActive();
  Serial.printf("[trigger] manual open requested from the guard view -> %s\n", doorActive ? "FIRED" : "ignored (door inactive)");
  if (doorActive) {
    Relay.pulse(Network.config().relayPulseMs);
    lastRelayGrantMs = millis();
  }

  LogEvent ev;
  ev.credentialId = -1;
  ev.result = doorActive ? "granted" : "denied";
  ev.reason = "manual_trigger";
  ev.doorMode = Access.mode();
  ev.eventTimeIso = nowIso8601();
  PendingLogs.push(ev);

  if (Network.isConnected()) {
    BackendApi::uploadLogs();
  }
}

// Checks the (optional) door-position sensor for a debounced state change
// and reports it — inert entirely if PIN_DOOR_SENSOR is -1 (Sensor.poll()
// always returns false then). An opening is "forced" when it happens more
// than DOOR_OPEN_GRACE_MS after the last granted access/manual trigger —
// includes the case where the door opens with no grant since boot at all,
// since lastRelayGrantMs starts at 0. Reporting is best-effort with a short
// retry (SENSOR_REPORT_RETRY_MS) rather than queued: if the node is
// offline for the entire time the door is open, that particular episode is
// never reported once it closes again — same documented v1 tradeoff as the
// rest of this firmware's live, non-persisted reporting paths.
void checkDoorSensor() {
  if (!Sensor.enabled()) return;

  if (Sensor.poll()) {
    bool open = Sensor.isOpen();
    bool forced = open && (millis() - lastRelayGrantMs > DOOR_OPEN_GRACE_MS);
    Serial.printf("[sensor] door %s%s\n", open ? "opened" : "closed", forced ? " — FORCED, no recent granted access" : "");
    sensorReportPending = true;
    sensorReportOpenValue = open;
    sensorReportForcedValue = forced;
    lastSensorReportAttemptMs = 0;  // try to report this right away, below
  }

  if (!sensorReportPending || !Network.isConnected()) return;
  uint32_t now = millis();
  if (now - lastSensorReportAttemptMs < SENSOR_REPORT_RETRY_MS) return;
  lastSensorReportAttemptMs = now;
  if (BackendApi::reportSensor(sensorReportOpenValue, sensorReportForcedValue)) {
    sensorReportPending = false;
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
  Sensor.begin(PIN_DOOR_SENSOR, DOOR_SENSOR_CLOSED_HIGH, DOOR_SENSOR_DEBOUNCE_MS);  // no-op if PIN_DOOR_SENSOR is -1

  Network.begin();  // runs the config portal (and reboots) on first boot

  if (Network.isConnected()) {
    if (!TimeUtil::syncNtp(Network.config().tz)) {
      Serial.println("[main] NTP sync timed out — schedules may be evaluated against a wrong clock until it lands.");
    }
    BackendApi::sync();  // also seeds the mode, so the fast poll timer below just refreshes it
    lastSyncMs = millis();
    lastModePollMs = millis();
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

  if (Network.isConnected() && now - lastModePollMs >= MODE_POLL_INTERVAL_MS) {
    lastModePollMs = now;
    BackendApi::syncMode();
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

  checkManualTrigger();
  checkDoorSensor();
}
