/* <x0/TimeoutScheduler.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <tbb/concurrent_priority_queue.h>

namespace x0 {

/**
 * MPSC Concurrent Timeout Scheduler.
 *
 * Multiple Producer single consumer timeout scheduler, that means,
 * every thread can add timeouts concurrently but timeouts may only
 * be from within a single thread.
 */
class X0_API TimeoutScheduler {
private:
	struct Timeout {
		time_t when;
		std::function<void()> handler;

		Timeout() : when(0), handler() {}
		Timeout(time_t w, const std::function<void()>& h) : when(w), handler(h) {}

		bool expired(time_t now) const { return when <= now; }
		void fire() { when = 0; handler(); }

		bool operator<(const Timeout& v) const { return when > v.when; }
	};

	Timeout pending;
	tbb::concurrent_priority_queue<Timeout> queue;

public:
	/**
	 * Adds a timeout handler to the queue.
	 *
	 * \param when Absolute timestamp when to fire the event.
	 * \param handler Callback to invoke on fire.
	 */
	void push(time_t when, const std::function<void()>& handler);
	void pulse(time_t now);
};

} // namespace x0
