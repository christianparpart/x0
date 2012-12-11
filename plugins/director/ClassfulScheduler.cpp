/* <plugins/director/ClassfulScheduler.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "ClassfulScheduler.h"
#include "RequestNotes.h"
#include "Director.h"
#include "Backend.h"

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

// {{{ ClassfulScheduler impl
ClassfulScheduler::ClassfulScheduler(Director* d) :
	Scheduler(d),
	rootBucket_(new Bucket(this, nullptr, "_root", 100))
{
}

ClassfulScheduler::~ClassfulScheduler()
{
	delete rootBucket_;
}

ClassfulScheduler::Bucket* ClassfulScheduler::createBucket(const std::string& name, size_t rate, size_t ceil)
{
	return rootBucket_->createChild(name, rate, ceil);
}

ClassfulScheduler::Bucket* ClassfulScheduler::findBucket(const std::string& name) const
{
	return rootBucket_->findChild(name);
}

void ClassfulScheduler::schedule(x0::HttpRequest* r)
{
	TRACE("schedule()");

	auto notes = director()->requestNotes(r);

	r->responseHeaders.push_back("X-Director-Cluster", director()->name());

	// {{{ bucket lookup
	Bucket* bucket;

	if (notes->bucketName.empty()) {
		bucket = rootBucket_;
	} else {
		bucket = rootBucket_->findChild(notes->bucketName);

		if (!bucket) {
			bucket = rootBucket_;
		}
	}

	notes->bucket = bucket;
	// }}}

	if (!bucket->get()) {
		bucket->enqueue(r);
		return;
	}

	bool allDisabled = false;

	if (notes->backend) {
		if (notes->backend->healthMonitor()->isOnline()) {
			pass(r, notes, notes->backend);
		} else {
			// pre-selected a backend, but this one is not online, so generate a 503 to give the client some feedback
			r->log(Severity::error, "director: Requested backend '%s' is %s, and is unable to process requests.",
				notes->backend->name().c_str(), notes->backend->healthMonitor()->state_str().c_str());
			r->status = x0::HttpStatus::ServiceUnavailable;
			r->finish();
		}
	}
	else if (Backend* backend = findLeastLoad(BackendRole::Active, &allDisabled)) {
		pass(r, notes, backend);
	}
	else if (Backend* backend = findLeastLoad(BackendRole::Standby, &allDisabled)) {
		pass(r, notes, backend);
	}
	else if (bucket->queued().current() < director()->queueLimit() && !allDisabled) {
		bucket->enqueue(r);
	}
	else if (Backend* backend = findLeastLoad(BackendRole::Backup)) {
		pass(r, notes, backend);
	}
	else if (bucket->queued().current() < director()->queueLimit()) {
		bucket->enqueue(r);
	}
	else {
		r->log(Severity::error, "director: '%s' queue limit %zu reached. Rejecting request.", director()->name().c_str(), director()->queueLimit());
		r->status = HttpStatus::ServiceUnavailable;
		if (director()->retryAfter()) {
			char value[64];
			snprintf(value, sizeof(value), "%zu", director()->retryAfter().totalSeconds());
			r->responseHeaders.push_back("Retry-After", value);
		}
		r->finish();
	}
}

Backend* ClassfulScheduler::findLeastLoad(BackendRole role, bool* allDisabled)
{
	Backend* best = nullptr;
	size_t bestAvail = 0;
	size_t enabledAndOnline = 0;

	for (auto backend: director()->backendsWith(role)) {
		if (!backend->isEnabled() || !backend->healthMonitor()->isOnline()) {
			TRACE("findLeastLoad: skip %s (disabled)", backend->name().c_str());
			continue;
		}

		++enabledAndOnline;

		size_t l = backend->load().current();
		size_t c = backend->capacity();
		size_t avail = c - l;

#ifndef NDEBUG
		director()->worker()->log(Severity::debug, "findLeastLoad: test %s (%zi/%zi, %zi)", backend->name().c_str(), l, c, avail);
#endif

		if (avail > bestAvail) {
#ifndef NDEBUG
			director()->worker()->log(Severity::debug, " - select (%zi > %zi, %s, %s)", avail, bestAvail, backend->name().c_str(), best ? best->name().c_str() : "(null)");
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
		director()->worker()->log(Severity::debug, "selecting backend %s", best->name().c_str());
#endif
		return best;
	}

#ifndef NDEBUG
	director()->worker()->log(Severity::debug, "selecting backend (role %d) failed", static_cast<int>(role));
#endif

	return nullptr;
}

void ClassfulScheduler::pass(HttpRequest* r, RequestNotes* notes, Backend* backend)
{
	TRACE("pass(backend:%s)", backend->name().c_str());
	notes->backend = backend;

	++load_;

	if (backend->tryProcess(r))
		return;

	--load_;
}

void ClassfulScheduler::dequeueTo(Backend* backend)
{
	if (HttpRequest* r = rootBucket_->dequeue()) {
		r->post([this, backend, r]() {
#ifndef NDEBUG
			r->log(Severity::debug, "Dequeueing request to backend %s @ %s",
				backend->name().c_str(), director_->name().c_str());
#endif
			pass(r, director_->requestNotes(r), backend);
		});
	}
}

void ClassfulScheduler::writeJSON(x0::JsonWriter& json) const
{
	json.beginObject()
		// from base class
		.name("load")(load_)
		.name("queued")(queued_)
		// own properties
		.name("buckets")(*rootBucket_)
		.endObject();
}

bool ClassfulScheduler::load(x0::IniFile& settings)
{
	return true;
}

bool ClassfulScheduler::save(x0::Buffer& out)
{
	return true;
}
// }}}
// {{{ ClassfulScheduler::Bucket impl
ClassfulScheduler::Bucket::Bucket(ClassfulScheduler* scheduler, Bucket* parent, const std::string& name, size_t rate, size_t ceil) :
#if !defined(NDEBUG)
	x0::Logging("ClassfulScheduler.Bucket[%s, %s]",
		scheduler->director()->name().c_str(),
		name.c_str()),
#endif
	scheduler_(scheduler),
	parent_(parent),
	name_(name),
	rate_(rate),
	ceil_(ceil ? ceil : rate),
	available_(ceil_),
	children_(),
	load_(),
	queued_(),
	queue_(),
	queueTimer_(),
	dequeueOffset_(0)
{
	if (parent_) {
		rate_ = parent_->get(rate_);
	}
}

ClassfulScheduler::Bucket::~Bucket()
{
	for (auto n: children_) {
		delete n;
	}

	if (parent_) {
		parent_->put(rate_);
	}
}

ClassfulScheduler::Bucket* ClassfulScheduler::Bucket::createChild(const std::string& name, size_t rate, size_t ceil)
{
	Bucket* b = new Bucket(scheduler_, this, name, rate, ceil);
	children_.push_back(b);
	return b;
}

ClassfulScheduler::Bucket* ClassfulScheduler::Bucket::findChild(const std::string& name) const
{
	for (auto n: children_)
		if (n->name() == name)
			return n;

	for (auto n: children_)
		if (Bucket* b = n->findChild(name))
			return b;

	return nullptr;
}


size_t ClassfulScheduler::Bucket::get(size_t n)
{
	// do not allow exceeding the buckets ceil
	if (actualRate() + n > ceil())
		return 0;

	// fits the requested value into rate?
	if (actualRate() + n <= rate()) {
		available_ -= n;
		return n;
	}

	// borrow the higher value from parent
	if (parent_) {
		size_t result = parent_->get(n);
		available_ -= result;
		return result;
	}

	return 0;
}

void ClassfulScheduler::Bucket::put(size_t n)
{
	assert(actualRate() + n <= ceil());

	size_t overrate = overRate();
	if (overrate) {
		size_t red = std::min(overrate, n);
		size_t green = n - red;
		parent_->put(red);
		available_ += green;
	} else {
		available_ += n;
	}
}

void ClassfulScheduler::Bucket::enqueue(x0::HttpRequest* r)
{
	queue_.push_back(r);

	++scheduler_->queued_;
	++queued_;

	updateQueueTimer();
}

/**
 * Dequeues a queued HTTP request.
 *
 * \return pointer to a dequeued HTTP request or nullptr if nothing queued.
 *
 * Must be invoked from within the director's thread context.
 */
HttpRequest* ClassfulScheduler::Bucket::dequeue()
{
	if (!children_.empty()) {
		dequeueOffset_ = dequeueOffset_ == 0
			? children_.size() - 1
			: dequeueOffset_ - 1;

		if (HttpRequest* r = children_[dequeueOffset_]->dequeue()) {
			return r;
		}
	}

	if (!queue_.empty()) {
		if (get()) {
			HttpRequest* r = queue_.front();
			queue_.pop_front();
			--queued_;
			return r;
		}
	}

	return nullptr;
}

void ClassfulScheduler::Bucket::writeJSON(x0::JsonWriter& json) const
{
	json.beginObject()
		.name("rate")(rate())
		.name("ceil")(ceil())
		.name("actual-rate")(actualRate())
		.name("load")(load())
		.name("queued")(queued())
		.endObject();
}

void ClassfulScheduler::Bucket::updateQueueTimer()
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
		auto notes = director()->requestNotes(r);
		TimeSpan age(director()->worker()->now() - notes->ctime);
		if (age < director()->queueTimeout())
			break;

		TRACE("updateQueueTimer: dequeueing timed out request");
		queue_.pop_front();
		--queued_;

		r->post([this, r]() {
			TRACE("updateQueueTimer: killing request with 503");

			r->status = HttpStatus::ServiceUnavailable;
			if (director()->retryAfter()) {
				char value[64];
				snprintf(value, sizeof(value), "%zu", director()->retryAfter().totalSeconds());
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
	auto notes = director()->requestNotes(r);
	TimeSpan age(director()->worker()->now() - notes->ctime);
	TimeSpan ttl(director()->queueTimeout() - age);
	TRACE("updateQueueTimer: starting new timer with ttl %f (%llu)", ttl.value(), ttl.totalMilliseconds());
	queueTimer_.start(ttl.value(), 0);
}
// }}}
