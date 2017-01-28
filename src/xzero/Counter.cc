// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Counter.h>
#include <xzero/JsonWriter.h>

namespace xzero {

Counter::Counter()
    : current_(0),
      max_(0),
      total_(0) {
}

Counter& Counter::operator++() {
  increment(1);
  return *this;
}

Counter& Counter::operator+=(size_t n) {
  increment(n);
  return *this;
}

Counter& Counter::operator--() {
  decrement(1);
  return *this;
}

Counter& Counter::operator-=(size_t n) {
  decrement(n);
  return *this;
}

bool Counter::increment(size_t n, size_t expected) {
  size_t desired = expected + n;

  if (!current_.compare_exchange_weak(expected, desired))
    return false;

  // XXX this might *not always* result into the highest value,
  // but we're fine with it.
  if (desired > max_.load())
    max_.store(expected + n);

  total_ += n;

  return true;
}

void Counter::increment(size_t n) {
  current_ += n;

  if (current_ > max_)
    max_.store(current_.load());

  total_ += n;
}

void Counter::decrement(size_t n) {
  current_ -= n;
}

template<>
JsonWriter& JsonWriter::value(const Counter& counter) {
  beginObject();
  name("current")(counter.current());
  name("max")(counter.max());
  name("total")(counter.total());
  endObject();

  return *this;
}

} // namespace xzero
