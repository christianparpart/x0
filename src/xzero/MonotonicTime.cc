// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/MonotonicTime.h>
#include <xzero/StringUtil.h>

namespace xzero {

std::string inspect(const MonotonicTime& value) {
  return StringUtil::format("$0", value.milliseconds());
}

std::ostream& operator<<(std::ostream& os, MonotonicTime value) {
  return os << inspect(value);
}

} // namespace xzero
