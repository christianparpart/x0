// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <fmt/format.h>
#include <xzero/Duration.h>

#include <ctime>
#include <ctime>
#include <inttypes.h>
#include <limits>
#include <optional>
#include <string>

namespace xzero {

class UnixTime {
 public:
  UnixTime(const UnixTime& t) = default;
  UnixTime& operator=(const UnixTime& t) = default;

  /**
   * Create a new UTC UnixTime instance with time = now
   */
  constexpr UnixTime() : utc_micros_{0} {};

  /**
   * Create a new UTC UnixTime instance
   *
   * @param timestamp the UTC microsecond timestamp
   */
  constexpr explicit UnixTime(uint64_t utc_time);

  /**
   * Create a new UTC UnixTime instance from a struct tm reference
   */
  explicit UnixTime(const struct tm& tm);

  /**
   * Create a new UTC UnixTime instance from given input date & time.
   *
   * @param year The absolute year
   * @param month the current month giving that @p year (1 == January).
   * @param day the day within the @p month (starting from 1).
   * @param hour the hours past the given @p day.
   * @param minute the minutes past the given @p hour.
   * @param second the number of seconds to the next @p minute.
   * @param millisecond number of milliseconds into the next @p second.
   * @param offset GMT-offset in seconds
   * @param isdst true if in daylight-saving
   */
  UnixTime(int year, int month, int day,
           int hour, int minute, int second, int millisecond,
           long int offset, bool isdst);

  /**
   * Return a representation of the date as a string (strftime)
   *
   * @param fmt the strftime format string (optional)
   */
  std::string toString(const char* fmt = "%Y-%m-%d %H:%M:%S") const;

  inline std::string format(const char* fmt) const { return toString(fmt); }

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

} // namespace xzero

namespace std {
  template <> class numeric_limits<xzero::UnixTime> {
  public:
    static constexpr xzero::UnixTime (min)() noexcept {
      return xzero::UnixTime{ 0 };
    }
    static constexpr xzero::UnixTime (max)() noexcept {
#if defined(XZERO_OS_WIN32)
      return xzero::UnixTime{ UINT64_MAX };
#else
      // for some reason it didn't work on VS
      return xzero::UnixTime{ std::numeric_limits<uint64_t>::max() };
#endif
    }
  };
}

#include <xzero/UnixTime_impl.h>

namespace fmt {
  template<>
  struct formatter<xzero::UnixTime> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::UnixTime& v, FormatContext &ctx) {
      return format_to(ctx.begin(), "{}", v.unixtime());
    }
  };
}

