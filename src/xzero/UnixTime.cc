// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <string>
#include <ctime>
#include <xzero/UnixTime.h>
#include <xzero/inspect.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/ISO8601.h>

namespace xzero {

UnixTime::UnixTime() :
    utc_micros_(WallClock::unixMicros()) {
}

UnixTime::UnixTime(const CivilTime& civil) {
  uint64_t days = civil.day() - 1;

  for (auto i = 1970; i < civil.year(); ++i) {
    days += 365 + ISO8601::isLeapYear(i);
  }

  for (auto i = 1; i < civil.month(); ++i) {
    days += ISO8601::daysInMonth(civil.year(), i);
  }

  utc_micros_ =
      days * kMicrosPerDay +
      civil.hour() * kMicrosPerHour +
      civil.minute() * kMicrosPerMinute +
      civil.second() * kMicrosPerSecond +
      civil.millisecond() * 1000 +
      civil.offset() * kMicrosPerSecond * -1;
}

UnixTime& UnixTime::operator=(const UnixTime& other) {
  utc_micros_ = other.utc_micros_;
  return *this;
}

UnixTime UnixTime::now() {
  return UnixTime(WallClock::unixMicros());
}

UnixTime UnixTime::daysFromNow(double days) {
  return UnixTime(WallClock::unixMicros() + (days * kMicrosPerDay));
}

std::string UnixTime::toString(const char* fmt) const {
  struct tm tm;
  time_t tt = utc_micros_ / 1000000;
  gmtime_r(&tt, &tm); // FIXPAUL

  char buf[256]; // FIXPAUL
  buf[0] = 0;
  strftime(buf, sizeof(buf), fmt, &tm);

  return std::string(buf);
}

Option<UnixTime> UnixTime::parseString(
    const std::string& str,
    const char* fmt /* = "%Y-%m-%d %H:%M:%S" */) {
  return UnixTime::parseString(str.data(), str.size(), fmt);
}

Option<UnixTime> UnixTime::parseString(
    const char* str,
    size_t strlen,
    const char* fmt /* = "%Y-%m-%d %H:%M:%S" */) {
  auto ct = CivilTime::parseString(str, strlen, fmt);
  if (ct.isEmpty()) {
    return None();
  } else {
    return Some(UnixTime(ct.get()));
  }
}

template <>
std::string StringUtil::toString(UnixTime value) {
  return value.toString();
}

template <>
std::string inspect(const UnixTime& value) {
  return value.toString();
}

}

xzero::UnixTime
    std::numeric_limits<xzero::UnixTime>::min() {
  return xzero::UnixTime::epoch();
}

xzero::UnixTime
    std::numeric_limits<xzero::UnixTime>::max() {
  return xzero::UnixTime(std::numeric_limits<uint64_t>::max());
}
