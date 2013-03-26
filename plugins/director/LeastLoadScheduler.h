/* <plugins/director/LeastLoadScheduler.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "Scheduler.h"
#include "Backend.h"
#include "RequestNotes.h"
#include <x0/TimeSpan.h>
#include <deque>
#include <mutex>
#include <atomic>
#include <ev++.h>

class RequestNotes;

class LeastLoadScheduler :
	public Scheduler
{
private:
	std::deque<x0::HttpRequest*> queue_; //! list of queued requests.
	std::mutex queueLock_;
	ev::timer queueTimer_;

public:
	explicit LeastLoadScheduler(Director* d);
	~LeastLoadScheduler();

	virtual void schedule(x0::HttpRequest* r);
	virtual void dequeueTo(Backend* backend);

private:
	bool tryProcess(x0::HttpRequest* r, bool* allDisabled, BackendRole role);
	bool tryProcess(HttpRequest* r, Backend* backend);
	bool tryEnqueue(x0::HttpRequest* r);
	void updateQueueTimer();
	x0::HttpRequest* dequeue();
};
