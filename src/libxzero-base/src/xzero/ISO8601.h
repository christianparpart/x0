/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2015 Laura Schlimmer, Paul Asmuth
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <xzero/stdtypes.h>
#include <xzero/Option.h>
#include <xzero/CivilTime.h>

namespace xzero {

class ISO8601 {
public:

  static Option<CivilTime> parse(const String& str);

  static bool isLeapYear(uint16_t year);

  static uint8_t daysInMonth(uint16_t year, uint8_t month);

};

}
