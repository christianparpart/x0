/* <plugins/director/Scheduler.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "Scheduler.h"
#include "Backend.h"
#include "RequestNotes.h"
#include <x0/http/HttpWorker.h>

Scheduler::Scheduler(BackendList* backends) :
	backends_(backends)
{
}

Scheduler::~Scheduler()
{
}

// ChanceScheduler
SchedulerStatus ChanceScheduler::schedule(RequestNotes* rn)
{
	size_t unavailable = 0;

	for (auto backend: backends()) {
		switch (backend->tryProcess(rn)) {
		case SchedulerStatus::Success:
			return SchedulerStatus::Success;
		case SchedulerStatus::Unavailable:
			++unavailable;
			break;
		case SchedulerStatus::Overloaded:
			break;
		}
	}

	return unavailable < backends().size()
		? SchedulerStatus::Overloaded
		: SchedulerStatus::Unavailable;
}

// RoundRobinScheduler
SchedulerStatus RoundRobinScheduler::schedule(RequestNotes* rn)
{
	unsigned unavailable = 0;

	for (size_t count = 0, limit = backends().size(); count < limit; ++count, ++next_) {
		if (next_ >= limit)
			next_ = 0;

		switch (backends()[next_++]->tryProcess(rn)) {
		case SchedulerStatus::Success:
			return SchedulerStatus::Success;
		case SchedulerStatus::Unavailable:
			++unavailable;
			break;
		case SchedulerStatus::Overloaded:
			break;
		}
	}

	return backends().size() == unavailable ? SchedulerStatus::Unavailable : SchedulerStatus::Overloaded;
}
