// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_datetime_hpp
#define sw_x0_datetime_hpp 1

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/Api.h>
#include <string>
#include <ctime>
#include <pthread.h>
#include <ev++.h>

namespace x0 {

//! \addtogroup base
//@{

/** date/time object that understands unix timestamps
 *  as well as HTTP conform dates as used in Date/Last-Modified and other
 * headers.
 */
class X0_API DateTime {
 private:
  ev_tstamp value_;
  mutable Buffer http_;
  mutable Buffer htlog_;

  static time_t mktime(const char* v);

 public:
  DateTime();
  explicit DateTime(const BufferRef& http_v);
  explicit DateTime(const std::string& http_v);
  explicit DateTime(ev::tstamp v);
  ~DateTime();

  ev::tstamp value() const;
  std::time_t unixtime() const;
  const Buffer& http_str() const;
  const Buffer& htlog_str() const;

  void update();
  void update(ev::tstamp vvalue);
  DateTime& operator=(ev::tstamp value);
  DateTime& operator=(const DateTime& value);

  bool valid() const;

  static int compare(const DateTime& a, const DateTime& b);
};

X0_API bool operator==(const DateTime& a, const DateTime& b);
X0_API bool operator!=(const DateTime& a, const DateTime& b);
X0_API bool operator<=(const DateTime& a, const DateTime& b);
X0_API bool operator>=(const DateTime& a, const DateTime& b);
X0_API bool operator<(const DateTime& a, const DateTime& b);
X0_API bool operator>(const DateTime& a, const DateTime& b);

X0_API TimeSpan operator-(const DateTime& a, const DateTime& b);
X0_API DateTime operator+(const DateTime& a, const TimeSpan& b);
X0_API DateTime operator-(const DateTime& a, const TimeSpan& b);

// {{{ impl
inline time_t DateTime::mktime(const char* v) {
  struct tm tm;
  tm.tm_isdst = 0;

  if (strptime(v, "%a, %d %b %Y %H:%M:%S GMT", &tm))
    return ::mktime(&tm) - timezone;

  return 0;
}

inline ev::tstamp DateTime::value() const { return value_; }

inline std::time_t DateTime::unixtime() const {
  return static_cast<std::time_t>(value_);
}

inline void DateTime::update() {
  update(static_cast<ev::tstamp>(std::time(nullptr)));
}

inline void DateTime::update(ev::tstamp v) {
  if (value_ != v) {
    value_ = v;
    http_.clear();
    htlog_.clear();
  }
}

inline DateTime& DateTime::operator=(ev::tstamp value) {
  update(value);
  return *this;
}

inline DateTime& DateTime::operator=(const DateTime& dt) {
  update(dt.value());
  return *this;
}

inline bool DateTime::valid() const { return value_ != 0; }

inline int DateTime::compare(const DateTime& a, const DateTime& b) {
  return a.unixtime() - b.unixtime();
}

inline TimeSpan operator-(const DateTime& a, const DateTime& b) {
  ev::tstamp diff = a.value() - b.value();

  if (diff < 0) diff = -diff;

  return TimeSpan(diff);
}

inline DateTime operator+(const DateTime& a, const TimeSpan& b) {
  return DateTime(a.value() + b.value());
}

inline DateTime operator-(const DateTime& a, const TimeSpan& b) {
  return DateTime(a.value() - b.value());
}
// }}}

// {{{ compare operators
inline bool operator==(const DateTime& a, const DateTime& b) {
  return DateTime::compare(a, b) == 0;
}

inline bool operator!=(const DateTime& a, const DateTime& b) {
  return DateTime::compare(a, b) != 0;
}

inline bool operator<=(const DateTime& a, const DateTime& b) {
  return DateTime::compare(a, b) <= 0;
}

inline bool operator>=(const DateTime& a, const DateTime& b) {
  return DateTime::compare(a, b) >= 0;
}

inline bool operator<(const DateTime& a, const DateTime& b) {
  return DateTime::compare(a, b) < 0;
}

inline bool operator>(const DateTime& a, const DateTime& b) {
  return DateTime::compare(a, b) > 0;
}
// }}}

//@}

}  // namespace x0

#endif
