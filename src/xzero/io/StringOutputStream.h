// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/io/OutputStream.h>
#include <string>

namespace xzero {

class StringOutputStream : public OutputStream {
 public:
  StringOutputStream(std::string* buffer) : sink_(buffer) {}

  std::string* get() const { return sink_; }

  int write(const char* buf, size_t size) override;

 private:
  std::string* sink_;
};

} // namespace xzero
