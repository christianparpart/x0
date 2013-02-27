/* <plugins/director/Scheduler.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "Scheduler.h"
#include "Director.h"
#include "RequestNotes.h"
#include <x0/http/HttpWorker.h>

Scheduler::Scheduler(Director* d) :
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
 * \note Invoked by \p Director::release().
 */
void Scheduler::release()
{
	--load_;
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

void Scheduler::log(x0::LogMessage& msg)
{
	msg.addTag("scheduler");
	msg.addTag("director/%s", director()->name().c_str());
	director()->worker()->log(std::move(msg));
	// TODO use director()->log(...) instead, to addTag(director()->name() there)
}
