#include "AccessController.h"
#include "TimeUtil.h"

AccessController Access;

void AccessController::replaceCache(CachedCredential *items, size_t count) {
  if (count > MAX_CACHED_CREDENTIALS) count = MAX_CACHED_CREDENTIALS;
  for (size_t i = 0; i < count; i++) _items[i] = items[i];
  _count = count;
}

namespace {
bool dayListContains(const String &daysOfWeek, int today) {
  String needle = String(today);
  int start = 0;
  while (start <= (int)daysOfWeek.length()) {
    int comma = daysOfWeek.indexOf(',', start);
    String tok = (comma == -1) ? daysOfWeek.substring(start) : daysOfWeek.substring(start, comma);
    tok.trim();
    if (tok == needle) return true;
    if (comma == -1) break;
    start = comma + 1;
  }
  return false;
}
}  // namespace

AccessDecision AccessController::evaluate(const String &valueHash) const {
  if (!_doorActive) {
    return {AccessResult::DENIED, "door_inactive", -1};
  }

  const CachedCredential *match = nullptr;
  for (size_t i = 0; i < _count; i++) {
    if (_items[i].valueHash.equalsIgnoreCase(valueHash)) {
      match = &_items[i];
      break;
    }
  }
  if (!match) {
    return {AccessResult::DENIED, "unknown_credential", -1};
  }

  time_t nowUtc = time(nullptr);
  if (match->validFrom != 0 && nowUtc < match->validFrom) {
    return {AccessResult::DENIED, "not_yet_valid", (int32_t)match->credentialId};
  }
  if (match->validUntil != 0 && nowUtc > match->validUntil) {
    return {AccessResult::DENIED, "expired", (int32_t)match->credentialId};
  }

  struct tm local;
  localtime_r(&nowUtc, &local);

  if (match->daysOfWeek.length() > 0) {
    int today = TimeUtil::isoWeekday(local);
    if (!dayListContains(match->daysOfWeek, today)) {
      return {AccessResult::DENIED, "schedule", (int32_t)match->credentialId};
    }
  }

  if (match->timeStart.length() > 0 || match->timeEnd.length() > 0) {
    int nowMin = local.tm_hour * 60 + local.tm_min;
    int startMin = match->timeStart.length() ? TimeUtil::parseHm(match->timeStart) : 0;
    int endMin = match->timeEnd.length() ? TimeUtil::parseHm(match->timeEnd) : (23 * 60 + 59);
    if (startMin < 0) startMin = 0;
    if (endMin < 0) endMin = 23 * 60 + 59;
    if (nowMin < startMin || nowMin > endMin) {
      return {AccessResult::DENIED, "schedule", (int32_t)match->credentialId};
    }
  }

  return {AccessResult::GRANTED, nullptr, (int32_t)match->credentialId};
}
