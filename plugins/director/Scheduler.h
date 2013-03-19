/* <plugins/director/Scheduler.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Counter.h>
#include <x0/Severity.h>
#include <x0/LogMessage.h>

class Director;
class Backend;
class RequestNotes;

namespace x0 {
	class HttpRequest;
	class Buffer;
	class IniFile;
}

class Scheduler
{
protected:
	Director* director_;

	x0::Counter load_;
	x0::Counter queued_;
	std::atomic<unsigned long long> dropped_;

	friend class Backend;
	friend class Director;

public:
	Scheduler(Director* d);
	virtual ~Scheduler();

	Director* director() const { return director_; }
	const x0::Counter& load() const { return load_; }
	const x0::Counter& queued() const { return queued_; }

	/**
	 * Schedules given request for processing by a backend.
	 *
	 * \param r the request to schedule.
	 *
	 * \note <b>MUST</b> be invoked from within the requests worker thread.
	 */
	virtual void schedule(x0::HttpRequest* r) = 0;

	virtual void dequeueTo(Backend* backend) = 0;

	/**
	 * \note Invoked by \p Director::release().
	 */
	inline void release();

	virtual void writeJSON(x0::JsonWriter& json) const;

	virtual bool load(x0::IniFile& settings);
	virtual bool save(x0::Buffer& out);

	template<typename... Args>
	void log(x0::Severity s, const char* fmt, Args... args)
	{
		x0::LogMessage msg(s, fmt, args...);
		log(msg);
	}

	void log(x0::LogMessage& msg);
};

inline void Scheduler::release()
{
	--load_;
}

inline x0::JsonWriter& operator<<(x0::JsonWriter& json, const Scheduler& value)
{
	value.writeJSON(json);
	return json;
}

