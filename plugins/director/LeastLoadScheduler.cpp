/* <plugins/director/LeastLoadScheduler.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "LeastLoadScheduler.h"
#include "Director.h"
#include "Backend.h"

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

// TODO rename DirectorNotes to RequestNotes
// obsolete release() hook with ~RequestNotes-destructor body

LeastLoadScheduler::LeastLoadScheduler(Director* d) :
	Scheduler(d),
	queue_(),
	queueTimer_(d->worker().loop())
{
	queueTimer_.set<LeastLoadScheduler, &LeastLoadScheduler::updateQueueTimer>(this);
	ev_unref(d->worker().loop());
}

LeastLoadScheduler::~LeastLoadScheduler()
{
	ev_ref(director_->worker().loop());
}

/**
 * Schedules given request.
 *
 * \note MUST be invoked from within the requests thread.
 */
void LeastLoadScheduler::schedule(HttpRequest* r)
{
	r->responseHeaders.push_back("X-Director-Cluster", director_->name());

	auto notes = r->setCustomData<DirectorNotes>(this, director_->worker().now());

	bool allDisabled = false;

	if (notes->backend) {
		if (notes->backend->healthMonitor().isOnline()) {
			pass(r, notes, notes->backend);
		} else {
			// pre-selected a backend, but this one is not online, so generate a 503 to give the client some feedback
			r->log(Severity::error, "director: Requested backend '%s' is %s, and is unable to process requests.",
				notes->backend->name().c_str(), notes->backend->healthMonitor().state_str().c_str());
			r->status = x0::HttpStatus::ServiceUnavailable;
			r->finish();
		}
	}
	else if (Backend* backend = findLeastLoad(Backend::Role::Active, &allDisabled)) {
		pass(r, notes, backend);
	}
	else if (Backend* backend = findLeastLoad(Backend::Role::Standby, &allDisabled)) {
		pass(r, notes, backend);
	}
	else if (queue_.size() < director_->queueLimit() && !allDisabled) {
		enqueue(r);
	}
	else if (Backend* backend = findLeastLoad(Backend::Role::Backup)) {
		pass(r, notes, backend);
	}
	else if (queue_.size() < director_->queueLimit()) {
		enqueue(r);
	}
	else {
		r->log(Severity::error, "director: '%s' queue limit %zu reached. Rejecting request.", director_->name().c_str(), director_->queueLimit());
		r->status = HttpStatus::ServiceUnavailable;
		if (director_->retryAfter()) {
			char value[64];
			snprintf(value, sizeof(value), "%zu", director_->retryAfter().totalSeconds());
			r->responseHeaders.push_back("Retry-After", value);
		}
		r->finish();
	}
}

bool LeastLoadScheduler::reschedule(HttpRequest* r, Backend* backend)
{
	auto notes = r->customData<DirectorNotes>(this);

	--backend->load_;
	notes->backend = nullptr;

	TRACE("requeue (retry-count: %zi / %zi)", notes->retryCount, director_->maxRetryCount());

	if (notes->retryCount == director_->maxRetryCount()) {
		--load_;

		r->status = HttpStatus::ServiceUnavailable;
		r->finish();

		return false;
	}

	++notes->retryCount;

	backend = nextBackend(backend, r);
	if (backend != nullptr) {
		notes->backend = backend;
		++backend->load_;

		if (backend->process(r)) {
			return true;
		}
	}

	TRACE("requeue (retry-count: %zi / %zi): giving up", notes->retryCount, director_->maxRetryCount());

	--load_;
	enqueue(r);

	return false;
}

Backend* LeastLoadScheduler::nextBackend(Backend* backend, HttpRequest* r)
{
	auto& backends = director_->backendsWith(backend->role());
	auto i = std::find(backends.begin(), backends.end(), backend);

	if (i != backends.end()) {
		auto k = i;
		++k;

		for (; k != backends.end(); ++k) {
			if (backend->isEnabled() && backend->healthMonitor().isOnline() && (*k)->load().current() < (*k)->capacity())
				return *k;

			TRACE("nextBackend: skip %s", backend->name().c_str());
		}

		for (k = backends.begin(); k != i; ++k) {
			if (backend->isEnabled() && backend->healthMonitor().isOnline() && (*k)->load().current() < (*k)->capacity())
				return *k;

			TRACE("nextBackend: skip %s", backend->name().c_str());
		}
	}

	TRACE("nextBackend: no next backend chosen.");
	return nullptr;
}

void LeastLoadScheduler::pass(HttpRequest* r, DirectorNotes* notes, Backend* backend)
{
	notes->backend = backend;

	++load_;
	++backend->load_;

	backend->process(r);
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
 * \see schedule(), reschedule(), enqueue(), dequeueTo()
 */

void LeastLoadScheduler::release(Backend* backend)
{
	--load_;

	if (!backend->isTerminating())
		dequeueTo(backend);
	else
		backend->tryTermination();
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the backend for serving.
 *
 * \param backend the backend to pass the dequeued request to.
 * \todo thread safety (queue_)
 */
void LeastLoadScheduler::dequeueTo(Backend* backend)
{
	if (HttpRequest* r = dequeue()) {
		r->post([this, backend, r]() {
#ifndef NDEBUG
			r->log(Severity::debug, "Dequeueing request to backend %s @ %s",
				backend->name().c_str(), director_->name().c_str());
#endif
			pass(r, r->customData<DirectorNotes>(this), backend);
		});
	}
}

void LeastLoadScheduler::enqueue(HttpRequest* r)
{
#ifndef NDEBUG
	r->log(Severity::debug, "Director %s overloaded. Enqueueing request.", director_->name().c_str());
#endif

	queue_.push_back(r);
	++queued_;

	updateQueueTimer();
}

HttpRequest* LeastLoadScheduler::dequeue()
{
	if (!queue_.empty()) {
		HttpRequest* r = queue_.front();
		queue_.pop_front();
		--queued_;
		return r;
	}

	return nullptr;
}

Backend* LeastLoadScheduler::findLeastLoad(Backend::Role role, bool* allDisabled)
{
	Backend* best = nullptr;
	size_t bestAvail = 0;
	size_t enabledAndOnline = 0;

	for (auto backend: director_->backendsWith(role)) {
		if (!backend->isEnabled() || !backend->healthMonitor().isOnline()) {
			TRACE("findLeastLoad: skip %s (disabled)", backend->name().c_str());
			continue;
		}

		++enabledAndOnline;

		size_t l = backend->load().current();
		size_t c = backend->capacity();
		size_t avail = c - l;

#ifndef NDEBUG
		director_->worker().log(Severity::debug, "findLeastLoad: test %s (%zi/%zi, %zi)", backend->name().c_str(), l, c, avail);
#endif

		if (avail > bestAvail) {
#ifndef NDEBUG
			director_->worker().log(Severity::debug, " - select (%zi > %zi, %s, %s)", avail, bestAvail, backend->name().c_str(), best ? best->name().c_str() : "(null)");
#endif
			bestAvail = avail;
			best = backend;
		}
	}

	if (allDisabled != nullptr) {
		*allDisabled = enabledAndOnline == 0;
	}

	if (bestAvail > 0) {
#ifndef NDEBUG
		director_->worker().log(Severity::debug, "selecting backend %s", best->name().c_str());
#endif
		return best;
	}

#ifndef NDEBUG
	director_->worker().log(Severity::debug, "selecting backend (role %d) failed", static_cast<int>(role));
#endif

	return nullptr;
}

void LeastLoadScheduler::updateQueueTimer()
{
	TRACE("updateQueueTimer()");

	// quickly return if queue-timer is already running
	if (queueTimer_.is_active()) {
		TRACE("updateQueueTimer: timer is active, returning");
		return;
	}

	// finish already timed out requests
	while (!queue_.empty()) {
		HttpRequest* r = queue_.front();
		auto notes = r->customData<DirectorNotes>(this);
		TimeSpan age(director_->worker().now() - notes->ctime);
		if (age < director_->queueTimeout())
			break;

		TRACE("updateQueueTimer: dequeueing timed out request");
		queue_.pop_front();
		--queued_;

		r->post([this, r]() {
			TRACE("updateQueueTimer: killing request with 503");

			r->status = HttpStatus::ServiceUnavailable;
			if (director_->retryAfter()) {
				char value[64];
				snprintf(value, sizeof(value), "%zu", director_->retryAfter().totalSeconds());
				r->responseHeaders.push_back("Retry-After", value);
			}
			r->finish();
		});
	}

	if (queue_.empty()) {
		TRACE("updateQueueTimer: queue empty. not starting new timer.");
		return;
	}

	// setup queue timer to wake up after next timeout is reached.
	HttpRequest* r = queue_.front();
	auto notes = r->customData<DirectorNotes>(this);
	TimeSpan age(director_->worker().now() - notes->ctime);
	TimeSpan ttl(director_->queueTimeout() - age);
	TRACE("updateQueueTimer: starting new timer with ttl %f (%llu)", ttl.value(), ttl.totalMilliseconds());
	queueTimer_.start(ttl.value(), 0);
}

void LeastLoadScheduler::writeJSON(Buffer& out)
{
	out << "  \"load\": " << load_ << ",\n"
		<< "  \"queued\": " << queued_ << ",\n";
}

bool LeastLoadScheduler::load(x0::IniFile& settings)
{
	return true;
}

bool LeastLoadScheduler::save(x0::Buffer& out)
{
	return true;
}
