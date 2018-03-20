// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <iosfwd>

namespace x0d {

// TODO: probably call me ServiceState
enum class DaemonState {
  Inactive,
  Initializing,
  Running,
  Upgrading,
  GracefullyShuttingdown
};

std::ostream& operator<<(std::ostream& os, DaemonState state);

} // namespace x0d
