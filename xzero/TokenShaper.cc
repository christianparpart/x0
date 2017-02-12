// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/TokenShaper.h>
#include <xzero/StringUtil.h>

namespace xzero {

template<>
std::string StringUtil::toString(TokenShaperError ec) {
  switch (ec) {
    case TokenShaperError::Success: return "Success";
    case TokenShaperError::RateLimitOverflow: return "Rate Limit Overflow";
    case TokenShaperError::CeilLimitOverflow: return "Ceil Limit Overflow";
    case TokenShaperError::NameConflict: return "Name Conflict";
    case TokenShaperError::InvalidChildNode: return "Invalid Child Node";
  }
}

} // namespace xzero
