/* <plugins/director/Backend.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
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
Backend::Backend(Director* director,
	const std::string& name, const SocketSpec& socketSpec, size_t capacity,
	HealthMonitor* healthMonitor) :
#ifndef NDEBUG
	Logging("Backend/%s", name.c_str()),
#endif
	manager_(director),
	director_(director), // FIXME remove me
	name_(name),
	capacity_(capacity),
	load_(),
	lock_(),
	role_(Role::Active),
	enabled_(true),
	socketSpec_(socketSpec),
	healthMonitor_(healthMonitor)
{
	if (!healthMonitor_)
		return;

	healthMonitor_->setStateChangeCallback([&](HealthMonitor*) {
		director_->worker_->log(Severity::info, "Director '%s': backend '%s' is now %s.",
			director_->name().c_str(), name_.c_str(), healthMonitor_->state_str().c_str());

		if (healthMonitor_->isOnline()) {
			if (!director_->stickyOfflineMode()) {
				// try delivering a queued request
				director_->scheduler()->dequeueTo(this);
			} else {
				// disable backend due to sticky-offline mode
				director_->worker_->log(Severity::info, "Director '%s': backend '%s' disabled due to sticky offline mode.",
					director_->name().c_str(), name_.c_str());
				setEnabled(false);
			}
		}
	});
}

Backend::~Backend()
{
	delete healthMonitor_;
}

size_t Backend::capacity() const
{
	return capacity_;
}

void Backend::setCapacity(size_t value)
{
	capacity_ = value;
}

const std::string& Backend::role_str() const
{
	static const std::string str[] = {
		"active",
		"standby",
		"backup",
		"terminate",
	};

	return str[static_cast<unsigned>(role_)];
}

void Backend::writeJSON(JsonWriter& json) const
{
	static const std::string boolStr[] = { "false", "true" };

	json.beginObject()
		.name("name")(name_)
		.name("capacity")(capacity_)
		.name("enabled")(enabled_)
		.name("protocol")(protocol())
		.name("role")(role_str());

	if (socketSpec_.isInet()) {
		json.name("hostname")(socketSpec_.ipaddr().str())
			.name("port")(socketSpec_.port());
	} else {
		json.name("path")(socketSpec_.local());
	}

	json.name("load")(load_);
	json.name("health")(*healthMonitor_);
	json.endObject();
}

void Backend::setRole(Role value)
{
	director_->worker_->log(Severity::debug, "setRole(%d) (from %d)", value, role_);
	if (role_ != value) {
		director_->unlink(this);
		role_ = value;
		director_->link(this);

		if (role_ == Role::Terminate) {
			director_->post([this](){ tryTermination(); });
		}
	}
}

/**
 * \note MUST be invoked from within the director's worker thread.
 */
bool Backend::tryTermination()
{
	if (role_ != Role::Terminate)
		return false;

	healthMonitor_->stop();

	if (load().current() > 0)
		return false;

	delete this;
	return true;
}

void Backend::terminate()
{
	setRole(Role::Terminate);
}

void Backend::setState(HealthMonitor::State value)
{
	healthMonitor_->setState(value);
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
	std::lock_guard<std::mutex> _(lock_);

	if (healthMonitor() && !healthMonitor()->isOnline())
		return false;

	if (!isEnabled())
		return false;

	if (load_.current() >= capacity_)
		return false;

	auto notes = director_->requestNotes(r);

	notes->backend = this;
	++notes->tryCount;

	if (!process(r)) {
		setState(HealthMonitor::State::Offline);
		return false;
	}

	++load_;
	++director_->scheduler()->load_;

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
	director_->scheduler()->release(this);
}
