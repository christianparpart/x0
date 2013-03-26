#pragma once

enum class SchedulerStatus
{
	Unavailable,		//!< all backends are offline and/or disabled
	Success,			//!< backend accepted request
	Overloaded,			//!< backends available but overloaded
};

