// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/TokenShaper.h>
#include <xzero/logging.h>
#include <iostream>

namespace xzero {

std::ostream& operator<<(std::ostream& os, TokenShaperError ec) {
  switch (ec) {
    case TokenShaperError::Success: return os << "Success";
    case TokenShaperError::RateLimitOverflow: return os << "Rate Limit Overflow";
    case TokenShaperError::CeilLimitOverflow: return os << "Ceil Limit Overflow";
    case TokenShaperError::NameConflict: return os << "Name Conflict";
    case TokenShaperError::InvalidChildNode: return os << "Invalid Child Node";
    default:
      logFatal("Unknown TokenShaperError value.");
  }
}

} // namespace xzero
