// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/DaemonState.h>
#include <xzero/StringUtil.h>

namespace x0d {

std::ostream& operator<<(std::ostream& os, DaemonState state) {
  switch (state) {
    case DaemonState::Inactive: return os << "Inactive";
    case DaemonState::Initializing: return os << "Initializing";
    case DaemonState::Running: return os << "Running";
    case DaemonState::Upgrading: return os << "Upgrading";
    case DaemonState::GracefullyShuttingdown: return os << "GracefullyShuttingdown";
    default: return os << "UNKNOWN";
  }
}

} // namespace x0d
