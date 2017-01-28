// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Counter.h>

namespace xzero {

class LinkedCounter {
 public:
  explicit LinkedCounter(Counter* link) : link_(link) {}

  const Counter& link() const noexcept { return *link_; }
  const Counter& child() const noexcept { return self_; }

  LinkedCounter& operator++() { child_++; if (link_) *link_++; }
  LinkedCounter& operator--() { child_--; if (link_) *link_--; }

 private:
  Counter* link_;
  Counter child_;
};

} // namespace xzero
