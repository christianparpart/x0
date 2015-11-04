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
