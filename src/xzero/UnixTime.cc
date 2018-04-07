// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
#include <xzero/ISO8601.h>
#include <xzero/time_constants.h>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>

namespace xzero {

inline uint64_t getUnixMicros(const CivilTime& civil) {
  uint64_t days = civil.day() - 1;

  for (auto i = 1970; i < civil.year(); ++i) {
    days += 365 + ISO8601::isLeapYear(i);
  }

  for (auto i = 1; i < civil.month(); ++i) {
    days += ISO8601::daysInMonth(civil.year(), i);
  }

  return
      days * kMicrosPerDay +
      civil.hour() * kMicrosPerHour +
      civil.minute() * kMicrosPerMinute +
      civil.second() * kMicrosPerSecond +
      civil.millisecond() * 1000 +
      civil.offset() * kMicrosPerSecond * -1;
}

UnixTime::UnixTime() :
    utc_micros_(WallClock::unixMicros()) {
}

UnixTime::UnixTime(const CivilTime& civil) :
    utc_micros_(getUnixMicros(civil)) {
}

UnixTime& UnixTime::operator=(const UnixTime& other) {
  utc_micros_ = other.utc_micros_;
  return *this;
}

UnixTime UnixTime::now() {
  return UnixTime(WallClock::unixMicros());
}

UnixTime UnixTime::daysFromNow(double days) {
  return UnixTime{ static_cast<uint64_t>(WallClock::unixMicros() + (days * kMicrosPerDay)) };
}

std::string UnixTime::toString(const char* fmt) const {
#if defined(HAVE_GMTIME_R)
  struct tm tm;
  time_t tt = utc_micros_ / 1000000;
  gmtime_r(&tt, &tm); // FIXPAUL

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

std::optional<UnixTime> UnixTime::parseString(
    const std::string& str,
    const char* fmt /* = "%Y-%m-%d %H:%M:%S" */) {
  return UnixTime::parseString(str.data(), str.size(), fmt);
}

std::optional<UnixTime> UnixTime::parseString(
    const char* str,
    size_t strlen,
    const char* fmt /* = "%Y-%m-%d %H:%M:%S" */) {
  std::optional<CivilTime> ct = CivilTime::parseString(str, strlen, fmt);
  if (!ct) {
    return std::nullopt;
  } else {
    return UnixTime(ct.value());
  }
}

} // namespace xzero
