#pragma once
#include <Arduino.h>
#include "config.h"

struct LogEvent {
  String valueHash;      // may be empty
  int32_t credentialId;  // -1 = unknown/omit
  String result;         // "granted" | "denied"
  String reason;         // may be empty
  String eventTimeIso;   // "YYYY-MM-DDTHH:MM:SSZ"
};

// Bounded FIFO of access events waiting to be uploaded. Deliberately
// RAM-only: it survives network outages between syncs (that's the whole
// point of periodic sync instead of a live check per swipe), but anything
// still queued is lost on power loss/reboot — a documented V1 tradeoff.
class EventQueue {
 public:
  void push(const LogEvent &event);
  size_t size() const { return _count; }
  bool empty() const { return _count == 0; }

  // Copies up to maxCount pending events into out (oldest first) without
  // removing them — call ackFront() once the server confirms receipt.
  size_t peek(LogEvent *out, size_t maxCount) const;
  void ackFront(size_t count);

 private:
  LogEvent _items[MAX_QUEUED_LOG_EVENTS];
  size_t _head = 0;
  size_t _count = 0;
};

extern EventQueue PendingLogs;
