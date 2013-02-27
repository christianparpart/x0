/* <plugins/director/Backend.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "Backend.h"
#include "BackendManager.h"
#include "Director.h"
#include "RequestNotes.h"
#include "HealthMonitor.h"

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

/**
 * Initializes the backend.
 *
 * \param director The director object to attach this backend to.
 * \param name name of this backend (must be unique within given director's backends).
 * \param socketSpec backend socket spec (hostname + port, or local unix domain path).
 * \param capacity number of requests this backend is capable of handling in parallel.
 * \param healthMonitor specialized health-monitor instanciation, which will be owned by this backend.
 */
Backend::Backend(BackendManager* bm,
	const std::string& name, const SocketSpec& socketSpec, size_t capacity, HealthMonitor* healthMonitor) :
#ifndef NDEBUG
	Logging("Backend/%s", name.c_str()),
#endif
	manager_(bm),
	name_(name),
	capacity_(capacity),
	load_(),
	lock_(),
	enabled_(true),
	socketSpec_(socketSpec),
	healthMonitor_(healthMonitor),
	jsonWriteCallback_()
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

Backend::~Backend()
{
	delete healthMonitor_;
	pthread_spin_destroy(&lock_);
}

void Backend::log(x0::LogMessage&& msg)
{
	msg.addTag(name_);
	manager_->log(std::move(msg));
}

size_t Backend::capacity() const
{
	return capacity_;
}

void Backend::setCapacity(size_t value)
{
	capacity_ = value;
}

void Backend::writeJSON(JsonWriter& json) const
{
	static const std::string boolStr[] = { "false", "true" };

	json.beginObject()
		.name("name")(name_)
		.name("capacity")(capacity_)
		.name("enabled")(enabled_)
		.name("protocol")(protocol());

	if (socketSpec_.isInet()) {
		json.name("hostname")(socketSpec_.ipaddr().str())
			.name("port")(socketSpec_.port());
	} else {
		json.name("path")(socketSpec_.local());
	}

	json.name("load")(load_);

	if (healthMonitor_) {
		json.name("health")(*healthMonitor_);
	}

	if (jsonWriteCallback_)
		jsonWriteCallback_(this, json);

	json.endObject();
}

void Backend::setJsonWriteCallback(const std::function<void(const Backend*, JsonWriter&)>& callback)
{
	jsonWriteCallback_ = callback;
}

void Backend::clearJsonWriteCallback()
{
	jsonWriteCallback_ = std::function<void(const Backend*, JsonWriter&)>();
}

void Backend::setState(HealthState value)
{
	if (healthMonitor_) {
		healthMonitor_->setState(value);
	}
}

/*!
 * Tries to processes given request on this backend.
 *
 * \param r the request to be processed
 *
 * It only processes the request if this backend is healthy, enabled and the load has not yet reached its capacity.
 * It then passes the request to the implementation specific \c process() method. If this fails to initiate
 * processing, this backend gets flagged as offline automatically, otherwise the load counters are increased accordingly.
 *
 * \note <b>MUST</b> be invoked from within the request's worker thread.
 */
bool Backend::tryProcess(HttpRequest* r)
{
	bool success = false;
	pthread_spin_lock(&lock_);

	if (healthMonitor_ && !healthMonitor_->isOnline())
		goto done;

	if (!isEnabled())
		goto done;

	if (capacity_ && load_.current() >= capacity_)
		goto done;

	success = pass(r);

done:
	pthread_spin_unlock(&lock_);
	return success;
}

bool Backend::pass(x0::HttpRequest* r)
{
	++load_;

	if (!process(r)) {
		setState(HealthState::Offline);
		--load_;
		return false;
	}

	return true;
}

/**
 * Invoked internally when a request has been fully processed.
 *
 * This decrements the load-statistics, and potentially
 * dequeues possibly enqueued requests to take over.
 */
void Backend::release()
{
	--load_;
	manager_->release(this);
}

/**
 * Invoked internally when this backend could not handle this request.
 *
 * This decrements the load-statistics and potentially
 * reschedules the request.
 */
void Backend::reject(x0::HttpRequest* r)
{
	--load_;

	// and set the backend's health state to offline, since it
	// doesn't seem to function properly
	setState(HealthState::Offline);

	manager_->reject(r);
}
