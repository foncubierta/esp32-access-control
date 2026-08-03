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
  // BackendApi::sync() writes new entries directly into this buffer
  // (capacity cacheCapacity()) and then calls commitCache() — this table
  // is already the single largest static allocation in the firmware,
  // building a second MAX_CACHED_CREDENTIALS-sized array elsewhere just to
  // copy from it doesn't fit in DRAM alongside everything else (confirmed:
  // that used to be exactly what BackendApi::sync() did, and it overflowed
  // the link by ~16KB).
  CachedCredential *cacheBuffer() { return _items; }
  static constexpr size_t cacheCapacity() { return MAX_CACHED_CREDENTIALS; }
  void commitCache(size_t count) { _count = (count > MAX_CACHED_CREDENTIALS) ? MAX_CACHED_CREDENTIALS : count; }

  size_t cachedCount() const { return _count; }

  void setDoorActive(bool active) { _doorActive = active; }
  bool doorActive() const { return _doorActive; }

  // "auto" | "open" | "closed" | "identify" — set by the guard view via
  // /api/doors/:id, read by the node via the fast /api/node/mode poll.
  // Deliberately kept separate from evaluate(): the access decision below
  // always reflects the true permission check, and main.cpp applies the
  // mode as the final gate on whether the relay actually fires — so the
  // log always records what the credential was really entitled to, even
  // when the mode overrode it.
  void setMode(const String &mode) { _mode = mode; }
  String mode() const { return _mode; }

  // Bumped server-side on every guard "open now" click. main.cpp compares
  // this against the last value it acted on and fires a single pulse
  // whenever it changes — see checkManualTrigger() there for the full
  // baseline-on-first-read reasoning.
  void setTriggerSeq(int32_t seq) { _triggerSeq = seq; }
  int32_t triggerSeq() const { return _triggerSeq; }

  // True while the admin's "Leer tarjeta" button is waiting on this door's
  // reader. main.cpp reports the next raw scan to the backend when this is
  // set — see checkManualTrigger()'s sibling in main.cpp for the flow.
  void setEnrollArmed(bool armed) { _enrollArmed = armed; }
  bool enrollArmed() const { return _enrollArmed; }

  AccessDecision evaluate(const String &valueHash) const;

 private:
  CachedCredential _items[MAX_CACHED_CREDENTIALS];
  size_t _count = 0;
  bool _doorActive = true;
  String _mode = "auto";
  int32_t _triggerSeq = 0;
  bool _enrollArmed = false;
};

extern AccessController Access;
