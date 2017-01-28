// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroState.h>
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
