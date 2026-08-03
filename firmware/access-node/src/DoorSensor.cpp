#include "DoorSensor.h"

DoorSensor Sensor;

void DoorSensor::begin(int8_t pin, bool closedIsHigh, uint32_t debounceMs) {
  _pin = pin;
  _closedIsHigh = closedIsHigh;
  _debounceMs = debounceMs;
  if (_pin < 0) return;

  pinMode(_pin, INPUT_PULLUP);
  bool rawHigh = digitalRead(_pin) == HIGH;
  _open = _closedIsHigh ? !rawHigh : rawHigh;
  _rawLast = rawHigh;
  _rawSinceMs = millis();
  _initialized = true;
}

bool DoorSensor::poll() {
  if (_pin < 0 || !_initialized) return false;

  bool rawHigh = digitalRead(_pin) == HIGH;
  uint32_t now = millis();

  if (rawHigh != _rawLast) {
    _rawLast = rawHigh;
    _rawSinceMs = now;
    return false;  // just started moving — wait for it to settle
  }

  bool debouncedOpen = _closedIsHigh ? !rawHigh : rawHigh;
  if (debouncedOpen == _open) return false;               // already reflects this reading
  if (now - _rawSinceMs < _debounceMs) return false;       // hasn't been stable long enough yet

  _open = debouncedOpen;
  return true;
}
