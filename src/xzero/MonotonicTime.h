// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <stdint.h>
#include <xzero/Duration.h>

namespace xzero {

class MonotonicTime {
public:
  constexpr MonotonicTime();
  constexpr MonotonicTime(const MonotonicTime& other);
  constexpr explicit MonotonicTime(uint64_t nanosecs);

  enum ZeroType { Zero };
  constexpr MonotonicTime(ZeroType zero);

  constexpr uint64_t seconds() const;
  constexpr uint64_t milliseconds() const;
  constexpr uint64_t microseconds() const;
  constexpr uint64_t nanoseconds() const;

  constexpr Duration operator-(const MonotonicTime& other) const;
  constexpr MonotonicTime operator+(const Duration& other) const;

  constexpr bool operator==(const MonotonicTime& other) const;
  constexpr bool operator!=(const MonotonicTime& other) const;
  constexpr bool operator<=(const MonotonicTime& other) const;
  constexpr bool operator>=(const MonotonicTime& other) const;
  constexpr bool operator<(const MonotonicTime& other) const;
  constexpr bool operator>(const MonotonicTime& other) const;

  constexpr bool operator!() const;

private:
  uint64_t nanosecs_;
};

std::string inspect(const MonotonicTime& value);

} // namespace xzero

#include <xzero/MonotonicTime_impl.h>
