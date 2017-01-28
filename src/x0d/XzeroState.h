// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace x0d {

// TODO: probably call me ServiceState
enum class XzeroState {
  Inactive,
  Initializing,
  Running,
  Upgrading,
  GracefullyShuttingdown
};

} // namespace x0d
