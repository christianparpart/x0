/* <plugins/director/Scheduler.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "Scheduler.h"
#include "Director.h"

Scheduler::Scheduler(Director* d) :
#if !defined(NDEBUG)
	x0::Logging("Scheduler/%s", d->name().c_str()),
#endif
	director_(d),
	load_(),
	queued_()
{
}

Scheduler::~Scheduler()
{
}
