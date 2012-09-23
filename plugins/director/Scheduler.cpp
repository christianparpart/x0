/* <plugins/director/Scheduler.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "Scheduler.h"
#include "Director.h"
#include "RequestNotes.h"
#include <x0/http/HttpWorker.h>

Scheduler::Scheduler(Director* d) :
#if !defined(NDEBUG)
	x0::Logging("Scheduler/%s", d->name().c_str()),
#endif
	director_(d),
	load_(),
	queued_(),
	dropped_(0)
{
}

Scheduler::~Scheduler()
{
}

/**
 * Notifies the director, that the given backend has just completed processing a request.
 *
 * \param backend the backend that just completed a request, and thus, is able to potentially handle one more.
 *
 * This method is to be invoked by backends, that
 * just completed serving a request, and thus, invoked
 * finish() on it, so it could potentially process
 * the next one, if, and only if, we have
 * already queued pending requests.
 *
 * Otherwise this call will do nothing.
 *
 * \see schedule(), dequeueTo()
 */
void Scheduler::release(Backend* backend)
{
	--load_;

	if (!backend->isTerminating())
		dequeueTo(backend);
	else
		backend->tryTermination();
}

void Scheduler::writeJSON(x0::JsonWriter& json) const
{
	json.beginObject()
		.name("load")(load_)
		.name("queued")(queued_)
		.name("dropped")(dropped_)
		.endObject();
}

bool Scheduler::load(x0::IniFile& settings)
{
	return true;
}

bool Scheduler::save(x0::Buffer& out)
{
	return true;
}
