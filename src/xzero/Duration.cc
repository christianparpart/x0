// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Duration.h>
#include <xzero/StringUtil.h>
#include <sstream>

namespace xzero {

std::string inspect(const Duration& value) {
  unsigned years = value.days() / kDaysPerYear;
  unsigned days = value.days() % kDaysPerYear;;
  unsigned hours = value.hours() % kHoursPerDay;
  unsigned minutes = value.minutes() % kMinutesPerHour;
  unsigned seconds = value.seconds() % kSecondsPerMinute;
  unsigned msecs = value.milliseconds () % kMillisPerSecond;

  std::stringstream sstr;
  int i = 0;

  if (years) {
    sstr << years << " years";
    i++;
  }

  if (days) {
    if (i++) sstr << ' ';
    sstr << days << " days";
  }

  if (hours) {
    if (i++) sstr << ' ';
    sstr << hours << " hours";
  }

  if (minutes) {
    if (i++) sstr << ' ';
    sstr << minutes << " minutes";
  }

  if (seconds) {
    if (i++) sstr << ' ';
    sstr << seconds << " seconds";
  }

  if (msecs) {
    if (i++) sstr << ' ';
    sstr << msecs << "ms";
  }

  if (!i) {
    sstr << "0s";
  }

  return sstr.str();
}

template<>
std::string StringUtil::toString<Duration>(Duration duration) {
  return inspect(duration);
}

template<>
std::string StringUtil::toString<const Duration&>(const Duration& duration) {
  return inspect(duration);
}

}
