// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>

#include <stdint.h>

#ifndef __APPLE__
#include <atomic>
#endif

namespace x0 {

class JsonWriter;

class X0_API Counter {
 public:
  typedef size_t value_type;

 private:
#ifndef __APPLE__
  std::atomic<value_type> current_;
  std::atomic<value_type> max_;
  std::atomic<value_type> total_;
#endif

 public:
  Counter();
  Counter(const Counter& other) = delete;
  Counter& operator=(const Counter& other) = delete;

  value_type operator()() const {
#ifdef __APPLE__
    return 0;
#else
    return current_.load();
#endif
  }

  value_type current() const {
#ifdef __APPLE__
    return 0;
#else
    return current_.load();
#endif
  }

  value_type max() const {
#ifdef __APPLE__
    return 0;
#else
    return max_.load();
#endif
  }

  value_type total() const {
#ifdef __APPLE__
    return 0;
#else
    return total_.load();
#endif
  }

  Counter& operator++();
  Counter& operator+=(size_t n);
  Counter& operator--();
  Counter& operator-=(size_t n);

  bool increment(size_t n, size_t expected);
  void increment(size_t n);
  void decrement(size_t n);
};

X0_API JsonWriter& operator<<(JsonWriter& json, const Counter& counter);

}  // namespace x0
