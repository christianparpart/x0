// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/Option.h>

namespace xzero {

/**
 * Class representing an instance of time in the gregorian calendar
 */
class CivilTime {
public:

  /**
   * Create a new CivilTime instance with all fields set to zero
   */
  constexpr CivilTime();

  /**
   * Create a new CivilTime instance with all fields set to zero
   */
  constexpr CivilTime(std::nullptr_t);

  /**
   * Parse time from the provided string
   *
   * @param str the string to parse
   * @param fmt the strftime format string (optional)
   */
  static Option<CivilTime> parseString(
      const std::string& str,
      const char* fmt = "%Y-%m-%d %H:%M:%S");

  /**
   * Parse time from the provided string
   *
   * @param str the string to parse
   * @param strlen the size of the string to parse
   * @param fmt the strftime format string (optional)
   */
  static Option<CivilTime> parseString(
      const char* str,
      size_t strlen,
      const char* fmt = "%Y-%m-%d %H:%M:%S");

  /**
   * Year including century / A.D. (eg. 1999)
   */
  constexpr uint16_t year() const;

  /**
   * Month [1-12]
   */
  constexpr uint8_t month() const;

  /**
   * Day of the month [1-31]
   */
  constexpr uint8_t day() const;

  /**
   * Hour [0-23]
   */
  constexpr uint8_t hour() const;

  /**
   * Hour [0-59]
   */
  constexpr uint8_t minute() const;

  /**
   * Second [0-60]
   */
  constexpr uint8_t second() const;

  /**
   * Millisecond [0-999]
   */
  constexpr uint16_t millisecond() const;

  /**
   * Timezone offset to UTC in seconds
   */
  constexpr int32_t offset() const;

  void setYear(uint16_t value);
  void setMonth(uint8_t value);
  void setDay(uint8_t value);
  void setHour(uint8_t value);
  void setMinute(uint8_t value);
  void setSecond(uint8_t value);
  void setMillisecond(uint16_t value);
  void setOffset(int32_t value);

protected:
  uint16_t year_;
  uint8_t month_;
  uint8_t day_;
  uint8_t hour_;
  uint8_t minute_;
  uint8_t second_;
  uint16_t millisecond_;
  int32_t offset_;
};

}

#include <xzero/CivilTime_impl.h>
