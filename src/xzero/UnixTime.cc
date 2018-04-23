// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <string>
#include <ctime>
#include <iostream>
#include <xzero/UnixTime.h>
#include <xzero/inspect.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/time_constants.h>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>

namespace xzero {

static uint64_t getUnixMicros(
    int year, int month, int day,
    int hour, int minute, int second, int millisecond,
    long int offset, bool isdst) {
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  tm.tm_year = year;
  tm.tm_mon = month;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
#if defined(XZERO_OS_UNIX)
  tm.tm_gmtoff = offset;
#endif
  tm.tm_isdst = isdst ? 1 : 0;

  const time_t gmt = mktime(&tm);
  const time_t utc = gmt + offset;

  return tm.tm_isdst == 1 ? (utc - 3600) * kMicrosPerSecond + millisecond * kMillisPerSecond
                          : utc * kMicrosPerSecond + millisecond * kMillisPerSecond;
}

UnixTime::UnixTime(const struct tm& tm) :
    UnixTime{tm.tm_year, tm.tm_mon, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, 0,
#if defined(XZERO_OS_UNIX)
             tm.tm_gmtoff,
#else
             0,
#endif
             tm.tm_isdst == 1} {
}

UnixTime::UnixTime(int year, int month, int day,
                   int hour, int minute, int second, int millisecond,
                   long int offset, bool isdst)
    : utc_micros_{getUnixMicros(year, month, day,
                                hour, minute, second, millisecond,
                                offset, isdst)} {
}

UnixTime UnixTime::now() {
  return UnixTime{WallClock::unixMicros()};
}

UnixTime UnixTime::daysFromNow(double days) {
  return UnixTime{ static_cast<uint64_t>(WallClock::unixMicros() + (days * kMicrosPerDay)) };
}

std::string UnixTime::toString(const char* fmt) const {
#if defined(HAVE_GMTIME_R)
  struct tm tm;
  time_t tt = utc_micros_ / kMicrosPerSecond;
  if (gmtime_r(&tt, &tm) == nullptr)
    return std::string{};

  char buf[256]; // FIXPAUL
  buf[0] = 0;
  strftime(buf, sizeof(buf), fmt, &tm);

  return std::string(buf);
#elif defined(XZERO_OS_WIN32) // FIXME(not working, why?) defined(HAVE_GMTIME_S)
  const time_t tt = utc_micros_ / 1000000;
  struct tm tm;
  gmtime_s(&tm, &tt);

  char buf[256];
  buf[0] = 0;
  size_t n = strftime(buf, sizeof(buf), fmt, &tm);

  return std::string(buf, n);
#else
  const time_t tt = utc_micros_ / 1000000;
  const struct tm* tm = gmtime(&tt);
  char buf[256];
  const size_t n = strftime(buf, sizeof(buf), fmt, tm);

  return std::string(buf, n);
#endif
}

} // namespace xzero
