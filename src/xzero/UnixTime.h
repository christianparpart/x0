/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _libxzero_UTIL_DATETIME_H
#define _libxzero_UTIL_DATETIME_H
#include <ctime>
#include <inttypes.h>
#include <limits>
#include <string>
#include <xzero/time_constants.h>
#include <xzero/CivilTime.h>
#include <xzero/Duration.h>
#include <xzero/Option.h>

namespace xzero {

class UnixTime {
public:
  /**
   * Create a new UTC UnixTime instance with time = now
   */
  UnixTime();

  /**
   * Create a new UTC UnixTime instance
   *
   * @param timestamp the UTC microsecond timestamp
   */
  constexpr UnixTime(uint64_t utc_time);

  /**
   * Create a new UTC UnixTime instance from a civil time reference
   *
   * @param civil the civil time
   */
  UnixTime(const CivilTime& civil);

  /**
   * Parse time from the provided string
   *
   * @param str the string to parse
   * @param fmt the strftime format string (optional)
   */
  static Option<UnixTime> parseString(
      const String& str,
      const char* fmt = "%Y-%m-%d %H:%M:%S");

  /**
   * Parse time from the provided string
   *
   * @param str the string to parse
   * @param strlen the size of the string to parse
   * @param fmt the strftime format string (optional)
   */
  static Option<UnixTime> parseString(
      const char* str,
      size_t strlen,
      const char* fmt = "%Y-%m-%d %H:%M:%S");

  /**
   * Return a representation of the date as a string (strftime)
   *
   * @param fmt the strftime format string (optional)
   */
  std::string toString(const char* fmt = "%Y-%m-%d %H:%M:%S") const;

  inline std::string format(const char* fmt) const { return toString(fmt); }

  UnixTime& operator=(const UnixTime& other);

  constexpr bool operator==(const UnixTime& other) const;
  constexpr bool operator!=(const UnixTime& other) const;
  constexpr bool operator<(const UnixTime& other) const;
  constexpr bool operator>(const UnixTime& other) const;
  constexpr bool operator<=(const UnixTime& other) const;
  constexpr bool operator>=(const UnixTime& other) const;

  /**
   * Calculates the duration between this time and the other.
   *
   * @param other the other time to calculate the difference from.
   *
   * @return the difference between this and the other time.
   */
  constexpr Duration operator-(const UnixTime& other) const;

  constexpr UnixTime operator+(const Duration& duration) const;
  constexpr UnixTime operator-(const Duration& duration) const;

  /**
   * Cast the UnixTime object to a UTC unix microsecond timestamp represented as
   * an uint64_t
   */
  constexpr explicit operator uint64_t() const;

  /**
   * Cast the UnixTime object to a UTC unix microsecond timestamp represented as
   * a double
   */
  constexpr explicit operator double() const;

  /**
   * Return the represented date/time as a UTC unix microsecond timestamp
   */
  constexpr uint64_t unixMicros() const;

  /**
   * Return the represented date/time as UTC in unix seconds timestamp.
   */
  constexpr time_t unixtime() const;

  /**
   * Return a new UnixTime instance with time 00:00:00 UTC, 1 Jan. 1970
   */
  static inline UnixTime epoch();

  /**
   * Return a new UnixTime instance with time = now
   */
  static UnixTime now();

  /**
   * Return a new UnixTime instance with time = now + days
   */
  static inline UnixTime daysFromNow(double days);

protected:

  /**
   * The utc microsecond timestamp of the represented moment in time
   */
  uint64_t utc_micros_;
};

}

namespace std {
template <> class numeric_limits<xzero::UnixTime> {
public:
  static xzero::UnixTime max();
  static xzero::UnixTime min();
};
}

#include <xzero/UnixTime_impl.h>
#endif
