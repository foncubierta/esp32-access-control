#pragma once
#include <Arduino.h>

#define WIEGAND_MAX_BITS 64

// Interrupt-driven Wiegand (D0/D1) reader. Works with any frame length —
// there's no single standard beyond 26-bit, and readers advertised as
// "Wiegand 34/37/58/..." all differ in how they'd need their parity bits
// interpreted. Rather than guessing per-format parity layouts, every frame
// is captured as a raw bit string and turned into a canonical
// "W<bitcount>:<HEX>" value — that's what gets hashed and matched against
// what an installer typed into the admin panel for that credential.
// Standard 26-bit frames are additionally decoded into facility/card
// fields, shown on Serial only as a convenience during enrollment.
class WiegandReader {
 public:
  void begin(uint8_t pinD0, uint8_t pinD1);

  // True once a full frame has been captured and is waiting to be consumed.
  bool available();

  // Consumes the pending frame.
  void getFrame(String &canonicalValue, uint8_t &bitCount, uint16_t &facilityCode, uint32_t &cardNumber);

  void IRAM_ATTR handleBit(uint8_t bit);

 private:
  static portMUX_TYPE _mux;

  volatile uint64_t _bits = 0;
  volatile uint8_t _bitCount = 0;
  volatile uint32_t _lastBitMicros = 0;
  volatile bool _frameReady = false;

  uint64_t _readyBits = 0;
  uint8_t _readyBitCount = 0;
};

extern WiegandReader Wiegand;

void IRAM_ATTR wiegandIsrD0();
void IRAM_ATTR wiegandIsrD1();
