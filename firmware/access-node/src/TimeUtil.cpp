#include "TimeUtil.h"

namespace {
// ESP32's Arduino toolchain uses a lightweight newlib, not glibc — timegm()
// (a GNU/BSD extension) isn't declared there, so it fails to link a UTC
// calendar->epoch conversion the normal way. This is Howard Hinnant's
// days_from_civil (http://howardhinnant.github.io/date_algorithms.html), a
// small, well-tested implementation that needs no timezone database, just
// calendar math — correct for any Gregorian calendar date.
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

time_t timegmPortable(const struct tm &t) {
  long days = daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  return (time_t)(days * 86400L + t.tm_hour * 3600L + t.tm_min * 60L + t.tm_sec);
}
}  // namespace

bool TimeUtil::syncNtp(const String &tz, uint32_t timeoutMs) {
  configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  uint32_t start = millis();
  // Anything before ~1971 means NTP hasn't landed yet — the epoch starts at 1970.
  while (now < 60L * 60 * 24 * 365 && millis() - start < timeoutMs) {
    delay(200);
    now = time(nullptr);
  }
  return now >= 60L * 60 * 24 * 365;
}

time_t TimeUtil::parseIso8601Utc(const String &iso) {
  if (iso.length() < 19) return 0;
  struct tm t = {};
  int y, mo, d, h, mi, s;
  if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return 0;
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_sec = s;
  return timegmPortable(t);  // interpret as UTC, matching the trailing 'Z'
}

int TimeUtil::parseHm(const String &hm) {
  if (hm.length() < 4) return -1;
  int h, m;
  if (sscanf(hm.c_str(), "%d:%d", &h, &m) != 2) return -1;
  return h * 60 + m;
}

int TimeUtil::isoWeekday(const struct tm &t) {
  return (t.tm_wday == 0) ? 6 : t.tm_wday - 1;
}
