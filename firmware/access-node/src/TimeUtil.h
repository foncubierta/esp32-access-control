#pragma once
#include <Arduino.h>
#include <time.h>

namespace TimeUtil {

// Starts NTP + sets the local timezone (POSIX TZ string), blocking briefly
// until a plausible wall-clock time is available. Call once the network
// transport is up. Returns false on timeout (schedules/validity windows
// will be evaluated against a wrong clock until it eventually syncs).
bool syncNtp(const String &tz, uint32_t timeoutMs = 15000);

// Parses "YYYY-MM-DDTHH:MM:SS[.ffffff]Z" (what FastAPI/Pydantic emits) into
// a UTC epoch. Returns 0 for an empty/unparseable input — callers treat 0
// as "no bound".
time_t parseIso8601Utc(const String &iso);

// "HH:MM" -> minutes since midnight, or -1 if empty/invalid.
int parseHm(const String &hm);

// 0=Mon..6=Sun, matching Permission.days_of_week's convention — glibc/newlib
// tm_wday is 0=Sunday, so this just rotates it.
int isoWeekday(const struct tm &t);

}  // namespace TimeUtil
