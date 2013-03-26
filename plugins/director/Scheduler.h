/* <plugins/director/Scheduler.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "SchedulerStatus.h"

#include <x0/Counter.h>
#include <x0/Severity.h>
#include <x0/LogMessage.h>

class Director;
class Backend;
class RequestNotes;

namespace x0 {
	class HttpRequest;
}

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

	virtual SchedulerStatus schedule(x0::HttpRequest* r) = 0;
};

class ChanceScheduler : public Scheduler {
public:
	explicit ChanceScheduler(BackendList* backends) : Scheduler(backends) {}

	virtual SchedulerStatus schedule(x0::HttpRequest* r);
};

class RoundRobinScheduler : public Scheduler {
public:
	explicit RoundRobinScheduler(BackendList* backends) : Scheduler(backends), next_(0) {}

	virtual SchedulerStatus schedule(x0::HttpRequest* r);

private:
	size_t next_;
};

