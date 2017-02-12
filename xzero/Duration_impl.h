// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/defines.h>

namespace xzero {

inline constexpr Duration::Duration(ZeroType)
    : micros_(0) {}

inline constexpr Duration::Duration(uint64_t microseconds)
    : micros_(microseconds) {}

inline Duration::Duration(const struct ::timeval& value)
    : micros_(value.tv_usec + value.tv_sec * kMicrosPerSecond) {}

inline Duration::Duration(const struct ::timespec& value)
    : micros_(value.tv_nsec / 1000 + value.tv_sec * kMicrosPerSecond) {}

inline Duration& Duration::operator=(const Duration& other) {
  micros_ = other.micros_;
  return *this;
}

constexpr bool Duration::operator==(const Duration& other) const {
  return micros_ == other.micros_;
}

constexpr bool Duration::operator!=(const Duration& other) const {
  return micros_ != other.micros_;
}

constexpr bool Duration::operator<(const Duration& other) const {
  return micros_ < other.micros_;
}

constexpr bool Duration::operator>(const Duration& other) const {
  return micros_ > other.micros_;
}

constexpr bool Duration::operator<=(const Duration& other) const {
  return micros_ <= other.micros_;
}

constexpr bool Duration::operator>=(const Duration& other) const {
  return micros_ >= other.micros_;
}

constexpr bool Duration::operator!() const {
  return micros_ == 0;
}

inline constexpr Duration::operator struct timeval() const {
#if defined(XZERO_OS_DARWIN)
  // OS/X plays in it's own universe. ;(
  return { static_cast<time_t>(micros_ / kMicrosPerSecond),
           static_cast<__darwin_suseconds_t>(micros_ % kMicrosPerSecond) };
#else
  return { static_cast<time_t>(micros_ / kMicrosPerSecond),
           static_cast<long>(micros_ % kMicrosPerSecond) };
#endif
}

inline constexpr Duration::operator struct timespec() const {
#if defined(XZERO_OS_DARWIN)
  // OS/X plays in it's own universe. ;(
  return { static_cast<time_t>(micros_ / kMicrosPerSecond),
           (static_cast<__darwin_suseconds_t>(micros_ % kMicrosPerSecond) * 1000) };
#else
  return { static_cast<time_t>(micros_ / kMicrosPerSecond),
           (static_cast<long>(micros_ % kMicrosPerSecond) * 1000) };
#endif
}

inline constexpr uint64_t Duration::microseconds() const noexcept {
  return micros_;
}

inline constexpr uint64_t Duration::milliseconds() const noexcept {
  return micros_ / kMillisPerSecond;
}

inline constexpr uint64_t Duration::seconds() const noexcept {
  return micros_ / kMicrosPerSecond;
}

inline constexpr Duration Duration::operator+(const Duration& other) const {
  return Duration(micros_ + other.micros_);
}

inline constexpr Duration Duration::operator-(const Duration& other) const {
  return micros_ > other.micros_
      ? Duration(micros_ - other.micros_)
      : Duration(other.micros_ - micros_);
}

inline constexpr Duration Duration::operator*(int factor) const {
  return Duration(micros_ * factor);
}

inline constexpr Duration Duration::operator/(int divisor) const {
  return Duration(micros_ / divisor);
}

inline constexpr uint64_t Duration::minutes() const noexcept {
  return seconds() / kSecondsPerMinute;
}

inline constexpr uint64_t Duration::hours() const noexcept {
  return minutes() / kMinutesPerHour;
}

inline constexpr uint64_t Duration::days() const noexcept {
  return hours() / kHoursPerDay;
}

constexpr Duration Duration::fromDays(uint64_t v) {
  return Duration(v * kMicrosPerSecond * kSecondsPerDay);
}

constexpr Duration Duration::fromHours(uint64_t v) {
  return Duration(v * kMicrosPerSecond * kSecondsPerHour);
}

constexpr Duration Duration::fromMinutes(uint64_t v) {
  return Duration(v * kMicrosPerSecond * kSecondsPerMinute);
}

constexpr Duration Duration::fromSeconds(uint64_t v) {
  return Duration(v * kMicrosPerSecond);
}

constexpr Duration Duration::fromMilliseconds(uint64_t v) {
  return Duration(v * 1000);
}

constexpr Duration Duration::fromMicroseconds(uint64_t v) {
  return Duration(v);
}

constexpr Duration Duration::fromNanoseconds(uint64_t v) {
  return Duration(v / 1000);
}

} // namespace xzero

constexpr xzero::Duration operator "" _microseconds(unsigned long long v) {
  return xzero::Duration::fromMicroseconds(v);
}

constexpr xzero::Duration operator "" _milliseconds(unsigned long long v) {
  return xzero::Duration::fromMilliseconds(v);
}

constexpr xzero::Duration operator "" _seconds(unsigned long long v) {
  return xzero::Duration::fromSeconds(v);
}

constexpr xzero::Duration operator "" _minutes(unsigned long long v) {
  return xzero::Duration::fromMinutes(v);
}

constexpr xzero::Duration operator "" _hours(unsigned long long v) {
  return xzero::Duration::fromHours(v);
}

constexpr xzero::Duration operator "" _days(unsigned long long v) {
  return xzero::Duration::fromDays(v);
}

constexpr xzero::Duration operator "" _years(unsigned long long v) {
  return xzero::Duration::fromDays(v * 365);
}
