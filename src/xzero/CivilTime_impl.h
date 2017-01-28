// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

namespace xzero {

inline constexpr CivilTime::CivilTime() :
    year_(0),
    month_(0),
    day_(0),
    hour_(0),
    minute_(0),
    second_(0),
    millisecond_(0),
    offset_(0) {}

inline constexpr CivilTime::CivilTime(std::nullptr_t) :
    CivilTime() {
}

inline constexpr uint16_t CivilTime::year() const {
  return year_;
}

inline constexpr uint8_t CivilTime::month() const {
  return month_;
}

inline constexpr uint8_t CivilTime::day() const {
  return day_;
}

inline constexpr uint8_t CivilTime::hour() const {
  return hour_;
}

inline constexpr uint8_t CivilTime::minute() const {
  return minute_;
}

inline constexpr uint8_t CivilTime::second() const {
  return second_;
}

inline constexpr uint16_t CivilTime::millisecond() const {
  return millisecond_;
}

inline constexpr int32_t CivilTime::offset() const {
  return offset_;
}

} // namespace xzero
