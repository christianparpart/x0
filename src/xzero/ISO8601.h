// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
