#include "RelayController.h"

RelayController Relay;

void RelayController::begin(uint8_t pin, bool activeHigh) {
  _pin = pin;
  _activeHigh = activeHigh;
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, _activeHigh ? LOW : HIGH);  // idle/off
}

void RelayController::pulse(uint32_t durationMs) {
  digitalWrite(_pin, _activeHigh ? HIGH : LOW);
  delay(durationMs);  // short and intentional — nothing else should run mid-pulse
  digitalWrite(_pin, _activeHigh ? LOW : HIGH);
}
