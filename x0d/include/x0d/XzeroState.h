#pragma once

#include <x0/sysconfig.h>
#include <x0/Api.h>

#include <string>

namespace x0d {

enum class XzeroState {
	Inactive,
	Initializing,
	Running,
	Upgrading,
	GracefullyShuttingdown
};

} // namespace x0
