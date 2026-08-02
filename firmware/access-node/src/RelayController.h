#pragma once
#include <Arduino.h>

// Drives the relay whose NO contact wires into the gate controller's
// START/PED dry-contact input (Aprimatic, BFT, ...).
class RelayController {
 public:
  void begin(uint8_t pin, bool activeHigh);
  void pulse(uint32_t durationMs);

 private:
  uint8_t _pin = 0;
  bool _activeHigh = true;
};

extern RelayController Relay;
