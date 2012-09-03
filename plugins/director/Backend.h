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
#include <x0/http/HttpRequest.h>

class Director;

/*!
 * \brief abstract base class for the actual proxying instances as used by \c Director.
 *
 * \see HttpBackend, FastCgiBackend
 */
class Backend
#ifndef NDEBUG
	: public x0::Logging
#endif
{
public:
	enum class Role {
		Active,
		Standby,
		Backup,
		Terminate,
	};

protected:
	Director* director_; //!< director, this backend is registered to

	std::string name_; //!< common name of this backend, for example: "appserver05"
	size_t capacity_; //!< number of concurrent requests being processable at a time.
	x0::Counter load_; //!< number of active (busy) connections

	Role role_; //!< backend role (Active or Standby)
	bool enabled_; //!< whether or not this director is enabled (default) or disabled (for example for maintenance reasons)
	x0::SocketSpec socketSpec_; //!< Backend socket spec.
	HealthMonitor* healthMonitor_; //!< health check timer

	friend class Director;

public:
	Backend(Director* director,
		const std::string& name, const x0::SocketSpec& socketSpec, size_t capacity,
		HealthMonitor* monitor);
	virtual ~Backend();

	virtual const std::string& protocol() const = 0;

	const std::string& name() const { return name_; }		//!< descriptive name of backend.
	Director* director() const { return director_; }		//!< pointer to the owning director.

	size_t capacity() const;								//!< number of requests this backend can handle in parallel.
	void setCapacity(size_t value);

	const x0::Counter& load() const { return load_; }		//!< number of currently being processed requests.

	const x0::SocketSpec& socketSpec() const { return socketSpec_; } //!< retrieves the backend socket spec

	// role
	Role role() const { return role_; }
	void setRole(Role value);
	const std::string& role_str() const;
	bool isTerminating() const { return role_ == Role::Terminate; }

	// enable/disable state
	void enable() { enabled_ = true; }
	bool isEnabled() const { return enabled_; }
	void setEnabled(bool value) { enabled_ = value; }
	void disable() { enabled_ = false; }

	// health state
	HealthMonitor::State healthState() const { return healthMonitor_->state(); }
	HealthMonitor& healthMonitor() { return *healthMonitor_; }

	//bool assign(x0::HttpRequest* r);
	void release();

	virtual void writeJSON(x0::JsonWriter& json) const;

	virtual void terminate();

protected:
	bool tryTermination();
	virtual bool process(x0::HttpRequest* r) = 0;

	friend class Scheduler;
	friend class LeastLoadScheduler;
	friend class ClassfulScheduler;

protected:
	void setState(HealthMonitor::State value);
};

X0_API inline x0::JsonWriter& operator<<(x0::JsonWriter& json, const Backend& backend)
{
	backend.writeJSON(json);
	return json;
}
