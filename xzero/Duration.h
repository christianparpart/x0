// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _XZERO_DURATION_H
#define _XZERO_DURATION_H

#include <ctime>
#include <inttypes.h>
#include <limits>
#include <string>
#include <sys/time.h>                   // struct timeval; struct timespec;
#include <xzero/time_constants.h>

namespace xzero {

class Duration {
private:
  enum class ZeroType { Zero };

public:
  constexpr static ZeroType Zero = ZeroType::Zero;

  /**
   * Creates a new Duration of zero microseconds.
   */
  constexpr Duration(ZeroType);

  /**
   * Create a new Duration
   *
   * @param microseconds the duration in microseconds
   */
  constexpr explicit Duration(uint64_t microseconds);

  /**
   * Creates a new Duration out of a @c timeval struct.
   *
   * @param value duration as @c timeval.
   */
  explicit Duration(const struct ::timeval& value);

  /**
   * Creates a new Duration out of a @c timespec struct.
   *
   * @param value duration as @c timespec.
   */
  explicit Duration(const struct ::timespec& value);

  Duration& operator=(const Duration& other);

  constexpr bool operator==(const Duration& other) const;
  constexpr bool operator!=(const Duration& other) const;
  constexpr bool operator<(const Duration& other) const;
  constexpr bool operator>(const Duration& other) const;
  constexpr bool operator<=(const Duration& other) const;
  constexpr bool operator>=(const Duration& other) const;
  constexpr bool operator!() const;

  constexpr Duration operator+(const Duration& other) const;
  constexpr Duration operator-(const Duration& other) const;
  constexpr Duration operator*(int factor) const;
  constexpr Duration operator/(int divisor) const;

  constexpr operator struct timeval() const;
  constexpr operator struct timespec() const;

  /**
   * Return the represented duration in microseconds
   */
  constexpr explicit operator uint64_t() const;

  /**
   * Return the represented duration in microseconds
   */
  constexpr explicit operator double() const;

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

  static constexpr Duration fromDays(uint64_t v);
  static constexpr Duration fromHours(uint64_t v);
  static constexpr Duration fromMinutes(uint64_t v);
  static constexpr Duration fromSeconds(uint64_t v);
  static constexpr Duration fromMilliseconds(uint64_t v);
  static constexpr Duration fromMicroseconds(uint64_t v);
  static constexpr Duration fromNanoseconds(uint64_t v);

protected:
  uint64_t micros_;
};

std::string inspect(const Duration& value);

} // namespace xzero

constexpr xzero::Duration operator "" _microseconds(unsigned long long v);
constexpr xzero::Duration operator "" _milliseconds(unsigned long long v);
constexpr xzero::Duration operator "" _seconds(unsigned long long v);
constexpr xzero::Duration operator "" _minutes(unsigned long long v);
constexpr xzero::Duration operator "" _hours(unsigned long long v);
constexpr xzero::Duration operator "" _days(unsigned long long v);
constexpr xzero::Duration operator "" _years(unsigned long long v);

#include <xzero/Duration_impl.h>
#endif
