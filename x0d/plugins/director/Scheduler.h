/* <plugins/director/Scheduler.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "SchedulerStatus.h"

#include <x0/Counter.h>
#include <x0/Severity.h>
#include <x0/LogMessage.h>

class Backend;
struct RequestNotes;

class Scheduler
{
public:
	typedef std::vector<Backend*> BackendList;

protected:
	BackendList* backends_;

protected:
	BackendList& backends() const { return *backends_; }

public:
	explicit Scheduler(BackendList* backends);
	virtual ~Scheduler();

	virtual SchedulerStatus schedule(RequestNotes* rn) = 0;
};

class ChanceScheduler : public Scheduler {
public:
	explicit ChanceScheduler(BackendList* backends) : Scheduler(backends) {}

	virtual SchedulerStatus schedule(RequestNotes* rn);
};

class RoundRobinScheduler : public Scheduler {
public:
	explicit RoundRobinScheduler(BackendList* backends) : Scheduler(backends), next_(0) {}

	virtual SchedulerStatus schedule(RequestNotes* rn);

private:
	size_t next_;
};

