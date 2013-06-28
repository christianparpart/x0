/* <x0/TimeoutScheduler.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/TimeoutScheduler.h>

namespace x0 {

void TimeoutScheduler::push(time_t when, const std::function<void()>& handler)
{
	queue.push(Timeout(when, handler));
}

void TimeoutScheduler::pulse(time_t now)
{
	if (pending.when) {
		if (!pending.expired(now))
			return;

		pending.fire();
	}

	while (queue.try_pop(pending) && pending.expired(now)) {
		pending.fire();
	}
}

} // namespace x0
