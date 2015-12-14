// This file is part of the "libcortex" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libcortex is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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
