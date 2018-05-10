// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/MatchClass.h>
#include <cassert>

namespace xzero::flow {

std::string tos(MatchClass mc) {
  switch (mc) {
    case MatchClass::Same:
      return "Same";
    case MatchClass::Head:
      return "Head";
    case MatchClass::Tail:
      return "Tail";
    case MatchClass::RegExp:
      return "RegExp";
    default:
      assert(!"FIXME: NOT IMPLEMENTED");
      return "<FIXME>";
  }
}

}  // namespace xzero::flow
