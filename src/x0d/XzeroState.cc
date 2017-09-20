// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroState.h>
#include <xzero/StringUtil.h>

namespace x0d {

std::ostream& operator<<(std::ostream& os, XzeroState state) {
  switch (state) {
    case XzeroState::Inactive: return os << "Inactive";
    case XzeroState::Initializing: return os << "Initializing";
    case XzeroState::Running: return os << "Running";
    case XzeroState::Upgrading: return os << "Upgrading";
    case XzeroState::GracefullyShuttingdown: return os << "GracefullyShuttingdown";
    default: return os << "UNKNOWN";
  }
}

} // namespace x0d
