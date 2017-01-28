// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

namespace xzero {

constexpr UnixTime::UnixTime(uint64_t utc_time) :
    utc_micros_(utc_time) {
}

constexpr bool UnixTime::operator==(const UnixTime& other) const {
  return utc_micros_ == other.utc_micros_;
}

constexpr bool UnixTime::operator!=(const UnixTime& other) const {
  return utc_micros_ != other.utc_micros_;
}

constexpr bool UnixTime::operator<(const UnixTime& other) const {
  return utc_micros_ < other.utc_micros_;
}

constexpr bool UnixTime::operator>(const UnixTime& other) const {
  return utc_micros_ > other.utc_micros_;
}

constexpr bool UnixTime::operator<=(const UnixTime& other) const {
  return utc_micros_ <= other.utc_micros_;
}

constexpr bool UnixTime::operator>=(const UnixTime& other) const {
  return utc_micros_ >= other.utc_micros_;
}

constexpr UnixTime::operator uint64_t() const {
  return utc_micros_;
}

constexpr UnixTime::operator double() const {
  return utc_micros_;
}

constexpr time_t UnixTime::unixtime() const {
  return utc_micros_ / kMicrosPerSecond;
}

constexpr uint64_t UnixTime::unixMicros() const {
  return utc_micros_;
}

UnixTime UnixTime::epoch() {
  return UnixTime(0);
}

constexpr Duration UnixTime::operator-(const UnixTime& other) const {
  return *this > other
      ? Duration(utc_micros_ - other.utc_micros_)
      : Duration(other.utc_micros_ - utc_micros_);
}

constexpr UnixTime UnixTime::operator+(const Duration& duration) const {
  return UnixTime(utc_micros_ + duration.microseconds());
}

constexpr UnixTime UnixTime::operator-(const Duration& duration) const {
  return UnixTime(utc_micros_ - duration.microseconds());
}

} // namespace xzero
