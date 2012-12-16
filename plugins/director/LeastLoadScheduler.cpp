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

#if 1 // !defined(NDEBUG)
#	define TRACE(msg...) { \
		char buf[4096]; \
		int n = snprintf(buf, sizeof(buf), "LeastLoadScheduler[%d]: ", x0::HttpWorker::currentId()); \
		snprintf(buf + n, sizeof(buf) - n, msg); \
		DEBUG("%s", buf); \
	}
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

LeastLoadScheduler::LeastLoadScheduler(Director* d) :
	Scheduler(d),
	queue_(),
	queueLock_(),
	queueTimer_(d->worker()->loop())
{
	queueTimer_.set<LeastLoadScheduler, &LeastLoadScheduler::updateQueueTimer>(this);
}

LeastLoadScheduler::~LeastLoadScheduler()
{
}

void LeastLoadScheduler::schedule(HttpRequest* r)
{
	auto notes = director_->requestNotes(r);
	bool allDisabled = false;

	if (notes->tryCount == 0) {
		r->responseHeaders.push_back("X-Director-Cluster", director_->name());

		if (notes->backend) {
			if (!tryProcess(r, notes->backend)) {
				// pre-selected a backend, but this one is not online, so generate a 503 to give the client some feedback
				r->log(Severity::error, "director: Requested backend '%s' is %s, and is unable to process requests.",
					notes->backend->name().c_str(), notes->backend->healthMonitor()->state_str().c_str());
				r->status = x0::HttpStatus::ServiceUnavailable;
				r->finish();

				// XXX Do NOT increment "dropped"-statistic, as most certainly directed requests are artificial
				// and the dropped statistic is more for the "real" users.
			}
			return;
		}
	} else {
		// rescheduling given request, so decrement the load counters
		--load_;

		notes->backend = nullptr;

		if (notes->tryCount >= director_->maxRetryCount() + 1) {
			r->log(Severity::info, "director: %s request failed %d times. Dropping.",
				director_->name().c_str(), notes->tryCount);

			++dropped_;

			r->status = HttpStatus::ServiceUnavailable;
			r->finish();
			return;
		}
	}

	if (tryProcess(r, &allDisabled, BackendRole::Active))
		return;

	if (tryProcess(r, &allDisabled, BackendRole::Standby))
		return;

	if (allDisabled && tryProcess(r, &allDisabled, BackendRole::Backup))
		return;

	if (tryEnqueue(r))
		return;

	r->log(Severity::info, "director: '%s' queue limit %zu reached. Rejecting request.",
		director_->name().c_str(), director_->queueLimit());

	++dropped_;

	r->status = HttpStatus::ServiceUnavailable;

	if (director_->retryAfter()) {
		char value[64];
		snprintf(value, sizeof(value), "%zu", director_->retryAfter().totalSeconds());
		r->responseHeaders.push_back("Retry-After", value);
	}

	r->finish();
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the backend for serving.
 *
 * \param backend the backend to pass the dequeued request to.
 */
void LeastLoadScheduler::dequeueTo(Backend* backend)
{
	if (HttpRequest* r = dequeue()) {
		r->post([this, backend, r]() {
#ifndef NDEBUG
			r->log(Severity::debug, "Dequeueing request to backend %s @ %s",
				backend->name().c_str(), director_->name().c_str());
#endif
			if (!tryProcess(r, backend)) {
				r->log(Severity::error, "Dequeueing request to backend %s @ %s failed.",
					backend->name().c_str(), director_->name().c_str());

				schedule(r);
			}
		});
	}
}

bool LeastLoadScheduler::tryEnqueue(HttpRequest* r)
{
	std::lock_guard<std::mutex> _(queueLock_);

	if (queue_.size() < director_->queueLimit()) {
		queue_.push_back(r);
		++queued_;

		r->log(Severity::info, "Director %s overloaded. Enqueueing request (%d).",
			director_->name().c_str(), queued_.current());

		director_->worker()->post([&]() { updateQueueTimer(); });
		return true;
	}

	return false;
}

HttpRequest* LeastLoadScheduler::dequeue()
{
	std::lock_guard<std::mutex> _(queueLock_);

	if (!queue_.empty()) {
		HttpRequest* r = queue_.front();
		queue_.pop_front();
		--queued_;

		r->log(Severity::debug, "Director %s dequeued request (%d left) (%llu).",
				director_->name().c_str(), queued_.current(), pthread_self());

		return r;
	}

	return nullptr;
}

#ifndef NDEBUG
inline const char* roleStr(BackendRole role)
{
	switch (role) {
		case BackendRole::Active: return "Active";
		case BackendRole::Standby: return "Standby";
		case BackendRole::Backup: return "Backup";
		case BackendRole::Terminate: return "Terminate";
		default: return "UNKNOWN";
	}
}
#endif

bool LeastLoadScheduler::tryProcess(x0::HttpRequest* r, bool* allDisabled, BackendRole role)
{
#ifndef NDEBUG
	director_->worker()->log(Severity::debug, "tryProcess(): role=%s", roleStr(role));
#endif

	Backend* best = nullptr;
	ssize_t bestAvail = 0;
	size_t enabledAndOnline = 0;

	for (auto backend: director_->backendsWith(role)) {
		if (!backend->isEnabled()) {
			//TRACE("tryProcess: skipping backend %s (disabled)", backend->name().c_str());
			continue;
		}

		if (!backend->healthMonitor()->isOnline()) {
			//TRACE("tryProcess: skipping backend %s (offline)", backend->name().c_str());
			continue;
		}

		++enabledAndOnline;

		ssize_t load = backend->load().current();
		ssize_t capacity = backend->capacity();
		ssize_t avail = capacity - load;

#ifndef NDEBUG
		director_->worker()->log(Severity::debug,
			"tryProcess: test backend %s (load:%zi, capacity:%zi, avail:%zi)",
			backend->name().c_str(), load, capacity, avail);
#endif

		if (avail > bestAvail) {
#ifndef NDEBUG
			director_->worker()->log(Severity::debug,
				"tryProcess: selecting backend %s (avail:%zi > bestAvail:%zi)",
				backend->name().c_str(), avail, bestAvail);
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
		director_->worker()->log(Severity::debug, "tryProcess: elected backend %s", best->name().c_str());
#endif
		return tryProcess(r, best);
	}

#ifndef NDEBUG
	director_->worker()->log(Severity::debug, "tryProcess: (role %s) failed scheduling request", roleStr(role));
#endif

	return false;
}

/**
 * Attempts to process request on given backend.
 */
bool LeastLoadScheduler::tryProcess(HttpRequest* r, Backend* backend)
{
	auto notes = director_->requestNotes(r);

	notes->backend = backend;
	++notes->tryCount;

	++load_;

	if (backend->tryProcess(r))
		return true;

	--load_;
	return false;
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
		auto notes = director_->requestNotes(r);
		TimeSpan age(r->connection.worker().now() - notes->ctime);

		TRACE("ctime(%s), now(%s), age(%s)",
			notes->ctime.http_str().c_str(),
			r->connection.worker().now().http_str().c_str(),
			age.str().c_str());

		if (age < director_->queueTimeout())
			break;

		TRACE("updateQueueTimer: dequeueing timed out request");
		queue_.pop_front();
		--queued_;

		r->post([this, r]() {
			TRACE("updateQueueTimer: killing request with 503");

			r->log(Severity::info, "Queued request timed out. Dropping.");
			r->status = HttpStatus::ServiceUnavailable;
			++dropped_;

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
	auto notes = director_->requestNotes(r);
	TimeSpan age(r->connection.worker().now() - notes->ctime);
	TimeSpan ttl(director_->queueTimeout() - age);
	TRACE("updateQueueTimer: starting new timer with ttl %f (%zu)", ttl.value(), ttl.totalMilliseconds());
	queueTimer_.start(ttl.value(), 0);
	director_->worker()->wakeup();
}

bool LeastLoadScheduler::load(x0::IniFile& settings)
{
	return true;
}

bool LeastLoadScheduler::save(x0::Buffer& out)
{
	return true;
}
