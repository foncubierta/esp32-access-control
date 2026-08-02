#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

struct CachedCredential {
  uint32_t credentialId = 0;
  String valueHash;    // lowercase hex, exactly as sent by the backend
  String daysOfWeek;   // "0,1,2,3,4" (Mon=0..Sun=6) or empty = every day
  String timeStart;    // "HH:MM" or empty = no lower bound
  String timeEnd;      // "HH:MM" or empty = no upper bound
  time_t validFrom = 0;   // 0 = no lower bound
  time_t validUntil = 0;  // 0 = no upper bound
};

enum class AccessResult : uint8_t { GRANTED, DENIED };

struct AccessDecision {
  AccessResult result;
  const char *reason;    // nullptr when granted
  int32_t credentialId;  // -1 when the hash matched nothing cached
};

// Holds the credential list handed back by the last successful
// /api/node/sync and decides locally whether a scanned hash is allowed
// through right now — this is what keeps the door working through a
// network outage between syncs.
class AccessController {
 public:
  void replaceCache(CachedCredential *items, size_t count);
  size_t cachedCount() const { return _count; }

  void setDoorActive(bool active) { _doorActive = active; }
  bool doorActive() const { return _doorActive; }

  AccessDecision evaluate(const String &valueHash) const;

 private:
  CachedCredential _items[MAX_CACHED_CREDENTIALS];
  size_t _count = 0;
  bool _doorActive = true;
};

extern AccessController Access;
