#include "AccessController.h"
#include "TimeUtil.h"

AccessController Access;

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

// A credential can appear more than once in the cache — once per grant
// (its own direct Permission, plus one per CredentialGroup it belongs to
// that has access to this door), each with its own schedule. Access is
// granted if *any* of those entries currently allows it; if none do, the
// reason reported is from whichever entry got furthest (so "schedule" beats
// "unknown_credential" when the hash matched something, just outside its
// window).
AccessDecision AccessController::evaluate(const String &valueHash) const {
  if (!_doorActive) {
    return {AccessResult::DENIED, "door_inactive", -1};
  }

  bool foundHash = false;
  int32_t matchedCredentialId = -1;
  const char *bestReason = "unknown_credential";

  time_t nowUtc = time(nullptr);
  struct tm local;
  localtime_r(&nowUtc, &local);
  int today = TimeUtil::isoWeekday(local);
  int nowMin = local.tm_hour * 60 + local.tm_min;

  for (size_t i = 0; i < _count; i++) {
    const CachedCredential &c = _items[i];
    if (!c.valueHash.equalsIgnoreCase(valueHash)) continue;
    foundHash = true;
    matchedCredentialId = (int32_t)c.credentialId;

    if (c.validFrom != 0 && nowUtc < c.validFrom) {
      bestReason = "not_yet_valid";
      continue;
    }
    if (c.validUntil != 0 && nowUtc > c.validUntil) {
      bestReason = "expired";
      continue;
    }
    if (c.daysOfWeek.length() > 0 && !dayListContains(c.daysOfWeek, today)) {
      bestReason = "schedule";
      continue;
    }
    if (c.timeStart.length() > 0 || c.timeEnd.length() > 0) {
      int startMin = c.timeStart.length() ? TimeUtil::parseHm(c.timeStart) : 0;
      int endMin = c.timeEnd.length() ? TimeUtil::parseHm(c.timeEnd) : (23 * 60 + 59);
      if (startMin < 0) startMin = 0;
      if (endMin < 0) endMin = 23 * 60 + 59;
      if (nowMin < startMin || nowMin > endMin) {
        bestReason = "schedule";
        continue;
      }
    }

    return {AccessResult::GRANTED, nullptr, matchedCredentialId};
  }

  if (!foundHash) {
    return {AccessResult::DENIED, "unknown_credential", -1};
  }
  return {AccessResult::DENIED, bestReason, matchedCredentialId};
}
