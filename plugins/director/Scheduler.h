/* <plugins/director/Scheduler.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include <x0/Counter.h>
#include <x0/Logging.h>

class Director;
class Backend;

namespace x0 {
	class HttpRequest;
	class Buffer;
	class IniFile;
}


class Scheduler
#ifndef NDEBUG
	: public x0::Logging
#endif
{
protected:
	Director* director_;

	x0::Counter load_;
	x0::Counter queued_;

	friend class Backend;

public:
	Scheduler(Director* d);
	virtual ~Scheduler();

	const x0::Counter& load() const { return load_; }
	const x0::Counter& queued() const { return queued_; }

	/**
	 * Schedules given request for processing by a backend.
	 *
	 * \param r the request to schedule.
	 *
	 * MUST be invoked from within the requests worker thread.
	 */
	virtual void schedule(x0::HttpRequest* r) = 0;

	/**
	 * reschedules given request.
	 *
	 * \param r the request to reschedule to another backend.
	 * \param backend the backend that failed to serve the request.
	 *
	 * MUST be invoked from within the requests worker thread.
	 */
	virtual bool reschedule(x0::HttpRequest* r, Backend* backend) = 0;

	virtual void release(Backend* backend) = 0;

	virtual void enqueue(x0::HttpRequest* r) = 0;
	virtual x0::HttpRequest* dequeue() = 0;
	virtual void dequeueTo(Backend* backend) = 0;

	virtual void writeJSON(x0::Buffer& out) = 0;

	virtual bool load(x0::IniFile& settings) = 0;
	virtual bool save(x0::Buffer& out) = 0;
};

