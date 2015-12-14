// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
