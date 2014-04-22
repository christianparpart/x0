/* <plugins/director/SchedulerStatus.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

/*!
 * Reflects the result of a request scheduling attempt.
 */
enum class SchedulerStatus
{
    Unavailable,		//!< Request not scheduled, as all backends are offline and/or disabled.
    Success,			//!< Request scheduled, Backend accepted request.
    Overloaded,			//!< Request not scheduled, as all backends available but overloaded or offline/disabled.
};

