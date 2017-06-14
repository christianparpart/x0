// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

namespace xzero {

inline constexpr MonotonicTime::MonotonicTime()
    : nanosecs_(0) {
}

inline constexpr MonotonicTime::MonotonicTime(const MonotonicTime& other)
    : nanosecs_(other.nanosecs_) {
}

inline constexpr MonotonicTime::MonotonicTime(uint64_t nanosecs)
    : nanosecs_(nanosecs) {
}

inline constexpr MonotonicTime::MonotonicTime(ZeroType zero)
    : nanosecs_(0) {
}

inline constexpr uint64_t MonotonicTime::seconds() const {
  return nanosecs_ / 1000000000;
}

inline constexpr uint64_t MonotonicTime::milliseconds() const {
  return nanosecs_ / 1000000;
}

inline constexpr uint64_t MonotonicTime::microseconds() const {
  return nanosecs_ / 1000;
}

inline constexpr uint64_t MonotonicTime::nanoseconds() const {
  return nanosecs_;
}

inline constexpr Duration MonotonicTime::operator-(const MonotonicTime& other) const {
  return other < *this
    ? Duration(microseconds() - other.microseconds())
    : Duration(other.microseconds() - microseconds());
}

inline constexpr MonotonicTime MonotonicTime::operator+(const Duration& other) const {
  return MonotonicTime(nanosecs_ + other.microseconds() * 1000);
}

inline constexpr bool MonotonicTime::operator==(const MonotonicTime& other) const {
  return nanosecs_ == other.nanosecs_;
}

inline constexpr bool MonotonicTime::operator!=(const MonotonicTime& other) const {
  return nanosecs_ != other.nanosecs_;
}

inline constexpr bool MonotonicTime::operator<=(const MonotonicTime& other) const {
  return nanosecs_ <= other.nanosecs_;
}

inline constexpr bool MonotonicTime::operator>=(const MonotonicTime& other) const {
  return nanosecs_ >= other.nanosecs_;
}

inline constexpr bool MonotonicTime::operator<(const MonotonicTime& other) const {
  return nanosecs_ < other.nanosecs_;
}

inline constexpr bool MonotonicTime::operator>(const MonotonicTime& other) const {
  return nanosecs_ > other.nanosecs_;
}

inline constexpr bool MonotonicTime::operator!() const {
  return nanosecs_ == 0;
}

} // namespace xzero
