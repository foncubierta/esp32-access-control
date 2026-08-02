#include "EventQueue.h"

EventQueue PendingLogs;

void EventQueue::push(const LogEvent &event) {
  if (_count >= MAX_QUEUED_LOG_EVENTS) {
    // Drop the oldest to make room for the newest — losing one old offline
    // event beats an unbounded queue or blocking the reader on a full one.
    _head = (_head + 1) % MAX_QUEUED_LOG_EVENTS;
    _count--;
  }
  size_t idx = (_head + _count) % MAX_QUEUED_LOG_EVENTS;
  _items[idx] = event;
  _count++;
}

size_t EventQueue::peek(LogEvent *out, size_t maxCount) const {
  size_t n = (maxCount < _count) ? maxCount : _count;
  for (size_t i = 0; i < n; i++) {
    out[i] = _items[(_head + i) % MAX_QUEUED_LOG_EVENTS];
  }
  return n;
}

void EventQueue::ackFront(size_t count) {
  if (count > _count) count = _count;
  _head = (_head + count) % MAX_QUEUED_LOG_EVENTS;
  _count -= count;
}
