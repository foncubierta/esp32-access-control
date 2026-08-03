#pragma once
#include <Arduino.h>

// Debounced reader for an optional door-position reed switch. Entirely
// inert (poll() always returns false, isOpen() always false) unless
// begin() is called with a real pin — main.cpp only does that when the
// configured sensor pin (RuntimeConfig::doorSensorPin, set via the config
// portal) isn't -1, so a node without the sensor wired never touches this
// pin or reports anything about it.
class DoorSensor {
 public:
  void begin(int8_t pin, bool closedIsHigh, uint32_t debounceMs);

  // Call every loop() iteration. Returns true exactly once when a
  // debounced state change is detected — check isOpen() right after to see
  // which way it went. No-op (always false) if begin() was never called.
  bool poll();

  bool enabled() const { return _pin >= 0; }
  bool isOpen() const { return _open; }

 private:
  int8_t _pin = -1;
  bool _closedIsHigh = true;
  uint32_t _debounceMs = 100;

  bool _open = false;        // debounced, reported state
  bool _rawLast = false;     // last raw pin reading, pre-debounce
  uint32_t _rawSinceMs = 0;  // millis() when _rawLast last changed
  bool _initialized = false;
};

extern DoorSensor Sensor;
