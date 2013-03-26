/* <plugins/director/LeastLoadScheduler.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "LeastLoadScheduler.h"
#include "Director.h"
#include "Backend.h"

#if !defined(XZERO_NDEBUG)
#	define TRACE(level, msg...) log(Severity::debug ## level, "http-request: " msg)
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

LeastLoadScheduler::LeastLoadScheduler(Director* d) :
	Scheduler(d),
	queue_(),
	queueLock_(),
	queueTimer_(d->worker()->loop())
{
	queueTimer_.set<LeastLoadScheduler, &LeastLoadScheduler::updateQueueTimer>(this);
}

LeastLoadScheduler::~LeastLoadScheduler()
{
}

