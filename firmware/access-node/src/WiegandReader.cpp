#include "WiegandReader.h"

WiegandReader Wiegand;
portMUX_TYPE WiegandReader::_mux = portMUX_INITIALIZER_UNLOCKED;

namespace {
// Gap between pulses that marks the end of a frame. Wiegand bits are
// normally a few dozen microseconds apart within a frame, so 25ms of
// silence is a safe "the card is done transmitting" threshold.
constexpr uint32_t FRAME_TIMEOUT_US = 25000;
}  // namespace

void WiegandReader::begin(uint8_t pinD0, uint8_t pinD1) {
  pinMode(pinD0, INPUT_PULLUP);
  pinMode(pinD1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinD0), wiegandIsrD0, FALLING);
  attachInterrupt(digitalPinToInterrupt(pinD1), wiegandIsrD1, FALLING);
}

void IRAM_ATTR WiegandReader::handleBit(uint8_t bit) {
  portENTER_CRITICAL_ISR(&_mux);
  uint32_t now = micros();
  if (_bitCount > 0 && (now - _lastBitMicros) > FRAME_TIMEOUT_US) {
    // Stale, unread bits from a previous frame — drop them instead of
    // merging into what's arriving now.
    _bits = 0;
    _bitCount = 0;
  }
  _lastBitMicros = now;
  if (_bitCount < WIEGAND_MAX_BITS) {
    _bits = (_bits << 1) | bit;
    _bitCount++;
  }
  portEXIT_CRITICAL_ISR(&_mux);
}

void IRAM_ATTR wiegandIsrD0() { Wiegand.handleBit(0); }
void IRAM_ATTR wiegandIsrD1() { Wiegand.handleBit(1); }

bool WiegandReader::available() {
  if (_frameReady) return true;
  if (_bitCount == 0) return false;

  portENTER_CRITICAL(&_mux);
  uint32_t now = micros();
  if (_bitCount > 0 && (now - _lastBitMicros) > FRAME_TIMEOUT_US) {
    _readyBits = _bits;
    _readyBitCount = _bitCount;
    _bits = 0;
    _bitCount = 0;
    _frameReady = true;
  }
  portEXIT_CRITICAL(&_mux);
  return _frameReady;
}

void WiegandReader::getFrame(String &canonicalValue, uint8_t &bitCount, uint16_t &facilityCode, uint32_t &cardNumber) {
  portENTER_CRITICAL(&_mux);
  uint64_t bits = _readyBits;
  uint8_t count = _readyBitCount;
  _frameReady = false;
  portEXIT_CRITICAL(&_mux);

  bitCount = count;
  facilityCode = 0;
  cardNumber = 0;
  canonicalValue = "";
  if (count == 0) return;

  uint64_t mask = (count >= 64) ? ~0ULL : ((1ULL << count) - 1);
  uint64_t value = bits & mask;

  if (count == 26) {
    // [even parity(1)] [facility(8)] [card(16)] [odd parity(1)] — shown on
    // Serial for convenience only, the match is always done on the raw bits.
    facilityCode = (value >> 17) & 0xFF;
    cardNumber = (value >> 1) & 0xFFFF;
  }

  uint8_t hexDigits = (count + 3) / 4;  // ceil(bits / 4)
  char hex[17];                          // WIEGAND_MAX_BITS(64)/4 + '\0'
  hex[hexDigits] = '\0';
  uint64_t v = value;
  for (int i = hexDigits - 1; i >= 0; --i) {
    hex[i] = "0123456789ABCDEF"[v & 0xF];
    v >>= 4;
  }

  canonicalValue = "W" + String(count) + ":" + String(hex);
}
