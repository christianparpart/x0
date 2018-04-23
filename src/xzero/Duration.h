// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <ctime>
#include <inttypes.h>
#include <limits>
#include <string>

#include <fmt/format.h>
#include <xzero/defines.h>
#include <xzero/time_constants.h>

#if defined(_WIN32) || defined(_WIN64)
#include <Winsock2.h>
#else
#include <sys/time.h>                   // struct timeval; struct timespec;
#endif

namespace xzero {

class Duration {
private:
  enum class ZeroType { Zero };

public:
  constexpr static ZeroType Zero = ZeroType::Zero;

  /**
   * Creates a new Duration of zero microseconds.
   */
  constexpr Duration(ZeroType) noexcept;

  /**
   * Create a new Duration
   *
   * @param microseconds the duration in microseconds
   */
  constexpr explicit Duration(uint64_t microseconds) noexcept;

  /**
   * Creates a new Duration out of a @c timeval struct.
   *
   * @param value duration as @c timeval.
   */
  constexpr Duration(const struct ::timeval& value) noexcept;

  /**
   * Creates a new Duration out of a @c timespec struct.
   *
   * @param value duration as @c timespec.
   */
  constexpr Duration(const struct ::timespec& value) noexcept;

  Duration& operator=(const Duration& other) noexcept;

  constexpr bool operator==(const Duration& other) const noexcept;
  constexpr bool operator!=(const Duration& other) const noexcept;
  constexpr bool operator<(const Duration& other) const noexcept;
  constexpr bool operator>(const Duration& other) const noexcept;
  constexpr bool operator<=(const Duration& other) const noexcept;
  constexpr bool operator>=(const Duration& other) const noexcept;
  constexpr bool operator!() const noexcept;

  constexpr Duration operator+(const Duration& other) const noexcept;
  constexpr Duration operator-(const Duration& other) const noexcept;
  constexpr Duration operator*(int factor) const noexcept;
  constexpr Duration operator/(int divisor) const noexcept;

  constexpr operator struct timeval() const noexcept;
  constexpr operator struct timespec() const noexcept;

  /**
   * Return the represented duration in microseconds
   */
  constexpr explicit operator uint64_t() const noexcept;

  /**
   * Return the represented duration in microseconds
   */
  constexpr explicit operator double() const noexcept;

  /**
   * Return the represented duration in microseconds
   */
  constexpr uint64_t microseconds() const noexcept;

  /**
   * Return the represented duration in milliseconds
   */
  constexpr uint64_t milliseconds() const noexcept;

  /**
   * Return the represented duration in seconds
   */
  constexpr uint64_t seconds() const noexcept;

  constexpr uint64_t minutes() const noexcept;
  constexpr uint64_t hours() const noexcept;
  constexpr uint64_t days() const noexcept;

  static constexpr Duration fromDays(uint64_t v) noexcept;
  static constexpr Duration fromHours(uint64_t v) noexcept;
  static constexpr Duration fromMinutes(uint64_t v) noexcept;
  static constexpr Duration fromSeconds(uint64_t v) noexcept;
  static constexpr Duration fromMilliseconds(uint64_t v) noexcept;
  static constexpr Duration fromMicroseconds(uint64_t v) noexcept;
  static constexpr Duration fromNanoseconds(uint64_t v) noexcept;

protected:
  uint64_t micros_;
};

std::string inspect(const Duration& value);

// {{{ inlines
inline constexpr Duration::Duration(ZeroType) noexcept
    : micros_(0) {}

inline constexpr Duration::Duration(uint64_t microseconds) noexcept
    : micros_(microseconds) {}

inline constexpr Duration::Duration(const struct ::timeval& value) noexcept
    : micros_(value.tv_usec + value.tv_sec * kMicrosPerSecond) {}

inline constexpr Duration::Duration(const struct ::timespec& value) noexcept
    : micros_(value.tv_nsec / 1000 + value.tv_sec * kMicrosPerSecond) {}

inline Duration& Duration::operator=(const Duration& other) noexcept {
  micros_ = other.micros_;
  return *this;
}

constexpr bool Duration::operator==(const Duration& other) const noexcept {
  return micros_ == other.micros_;
}

constexpr bool Duration::operator!=(const Duration& other) const noexcept {
  return micros_ != other.micros_;
}

constexpr bool Duration::operator<(const Duration& other) const noexcept {
  return micros_ < other.micros_;
}

constexpr bool Duration::operator>(const Duration& other) const noexcept {
  return micros_ > other.micros_;
}

constexpr bool Duration::operator<=(const Duration& other) const noexcept {
  return micros_ <= other.micros_;
}

constexpr bool Duration::operator>=(const Duration& other) const noexcept {
  return micros_ >= other.micros_;
}

constexpr bool Duration::operator!() const noexcept {
  return micros_ == 0;
}

inline constexpr Duration::operator struct timeval() const noexcept {
  return { static_cast<decltype(timeval::tv_sec)>(micros_ / kMicrosPerSecond),
           static_cast<decltype(timeval::tv_usec)>(micros_ % kMicrosPerSecond) };
}

inline constexpr Duration::operator struct timespec() const noexcept {
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

inline constexpr Duration Duration::operator+(const Duration& other) const noexcept {
  return Duration{micros_ + other.micros_};
}

inline constexpr Duration Duration::operator-(const Duration& other) const noexcept {
  return micros_ > other.micros_
      ? Duration{micros_ - other.micros_}
      : Duration{other.micros_ - micros_};
}

inline constexpr Duration Duration::operator*(int factor) const noexcept {
  return Duration{micros_ * factor};
}

inline constexpr Duration Duration::operator/(int divisor) const noexcept {
  return Duration{micros_ / divisor};
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

constexpr Duration Duration::fromDays(uint64_t v) noexcept {
  return Duration{v * kMicrosPerSecond * kSecondsPerDay};
}

constexpr Duration Duration::fromHours(uint64_t v) noexcept {
  return Duration{v * kMicrosPerSecond * kSecondsPerHour};
}

constexpr Duration Duration::fromMinutes(uint64_t v) noexcept {
  return Duration{v * kMicrosPerSecond * kSecondsPerMinute};
}

constexpr Duration Duration::fromSeconds(uint64_t v) noexcept {
  return Duration{v * kMicrosPerSecond};
}

constexpr Duration Duration::fromMilliseconds(uint64_t v) noexcept {
  return Duration{v * 1000};
}

constexpr Duration Duration::fromMicroseconds(uint64_t v) noexcept {
  return Duration{v};
}

constexpr Duration Duration::fromNanoseconds(uint64_t v) noexcept {
  return Duration{v / 1000};
}
// }}}

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

namespace fmt {
  template<>
  struct formatter<xzero::Duration> {
    using Duration = xzero::Duration;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const Duration& v, FormatContext &ctx) {
      return format_to(ctx.begin(), xzero::inspect(v));
    }
  };
}

