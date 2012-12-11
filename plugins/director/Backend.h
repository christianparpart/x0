/* <plugins/director/Backend.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include "HealthMonitor.h"

#include <x0/Counter.h>
#include <x0/Logging.h>
#include <x0/TimeSpan.h>
#include <x0/SocketSpec.h>
#include <x0/JsonWriter.h>
#include <x0/CustomDataMgr.h>
#include <x0/http/HttpRequest.h>

#include <mutex>

class BackendManager;
class Director;

/*!
 * \brief abstract base class for the actual proxying instances as used by \c BackendManager.
 *
 * \see BackendManager, HttpBackend, FastCgiBackend
 */
class Backend
#ifndef NDEBUG
	: public x0::Logging
#endif
{
	CUSTOMDATA_API_INLINE

protected:
	BackendManager* manager_; //!< manager, this backend is registered to

	std::string name_; //!< common name of this backend, for example: "appserver05"
	size_t capacity_; //!< number of concurrent requests being processable at a time.
	x0::Counter load_; //!< number of active (busy) connections

	std::mutex lock_; //!< scheduling mutex

	bool enabled_; //!< whether or not this backend is enabled (default) or disabled (for example for maintenance reasons)
	x0::SocketSpec socketSpec_; //!< Backend socket spec.
	HealthMonitor* healthMonitor_; //!< health check timer

	std::function<void(const Backend*, x0::JsonWriter&)> jsonWriteCallback_;

	friend class Director;

public:
	Backend(BackendManager* bm, const std::string& name, const x0::SocketSpec& socketSpec, size_t capacity, HealthMonitor* healthMonitor);
	virtual ~Backend();

	void setJsonWriteCallback(const std::function<void(const Backend*, x0::JsonWriter&)>& callback);
	void clearJsonWriteCallback();

	virtual const std::string& protocol() const = 0;

	const std::string& name() const { return name_; }		//!< descriptive name of backend.
	BackendManager* manager() const { return manager_; }	//!< manager instance that owns this backend.

	size_t capacity() const;								//!< number of requests this backend can handle in parallel.
	void setCapacity(size_t value);

	const x0::Counter& load() const { return load_; }		//!< number of currently being processed requests.

	const x0::SocketSpec& socketSpec() const { return socketSpec_; } //!< retrieves the backend socket spec

	// enable/disable state
	void enable() { enabled_ = true; }
	bool isEnabled() const { return enabled_; }
	void setEnabled(bool value) { enabled_ = value; }
	void disable() { enabled_ = false; }

	// health monitoring
	HealthMonitor* healthMonitor() { return healthMonitor_; }
	HealthState healthState() const { return healthMonitor_->state(); }

	bool tryProcess(x0::HttpRequest* r);
	bool pass(x0::HttpRequest* r);
	void release();
	void reject(x0::HttpRequest* r);

	virtual void writeJSON(x0::JsonWriter& json) const;

protected:
	/*!
	 * \brief initiates actual processing of given request.
	 *
	 * \note this method MUST NOT block.
	 */
	virtual bool process(x0::HttpRequest* r) = 0;

//	friend class Scheduler;
//	friend class LeastLoadScheduler;
//	friend class ClassfulScheduler;

protected:
	void setState(HealthState value);
};

X0_API inline x0::JsonWriter& operator<<(x0::JsonWriter& json, const Backend& backend)
{
	backend.writeJSON(json);
	return json;
}
