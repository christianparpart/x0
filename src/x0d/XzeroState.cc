#include "XzeroState.h"
#include <xzero/StringUtil.h>

namespace xzero {

using x0d::XzeroState;

template<> std::string StringUtil::toString(XzeroState state) {
  switch (state) {
    case XzeroState::Inactive: return "Inactive";
    case XzeroState::Initializing: return "Initializing";
    case XzeroState::Running: return "Running";
    case XzeroState::Upgrading: return "Upgrading";
    case XzeroState::GracefullyShuttingdown: return "GracefullyShuttingdown";
    default: return "UNKNOWN";
  }
}

} // namespace xzero
