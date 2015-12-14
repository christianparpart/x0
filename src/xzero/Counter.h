// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <stdint.h>
#include <atomic>

namespace xzero {

class JsonWriter;

class Counter {
 public:
  typedef size_t value_type;

 private:
  std::atomic<value_type> current_;
  std::atomic<value_type> max_;
  std::atomic<value_type> total_;

 public:
  Counter();
  Counter(const Counter& other) = delete;
  Counter& operator=(const Counter& other) = delete;

  value_type operator()() const {
    return current_.load();
  }

  value_type current() const {
    return current_.load();
  }

  value_type max() const {
    return max_.load();
  }

  value_type total() const {
    return total_.load();
  }

  Counter& operator++();
  Counter& operator+=(size_t n);
  Counter& operator--();
  Counter& operator-=(size_t n);

  bool increment(size_t n, size_t expected);
  void increment(size_t n);
  void decrement(size_t n);
};

JsonWriter& operator<<(JsonWriter& json, const Counter& counter);

}  // namespace xzero
