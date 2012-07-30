#include "Director.h"
#include "Backend.h"
#include "HttpBackend.h"

#include <x0/io/BufferSource.h>
#include <x0/StringTokenizer.h>
#include <x0/Url.h>
#include <fstream>

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

/**
 * Initializes a director object (load balancer instance).
 *
 * \param worker the worker associated with this director's local jobs (e.g. backend health checks).
 * \param name the unique human readable name of this director (load balancer) instance.
 */
Director::Director(HttpWorker* worker, const std::string& name) :
#ifndef NDEBUG
	Logging("Director/%s", name.c_str()),
#endif
	worker_(worker),
	name_(name),
	mutable_(false),
	backends_(),
	queue_(),
	queueLimit_(128),
	load_(),
	queued_(),
	lastBackend_(0),
	maxRetryCount_(6),
	storagePath_()
{
	backends_.resize(4);

	worker_->registerStopHandler(std::bind(&Director::onStop, this));
}

Director::~Director()
{
}

/**
 * Callback, invoked when the owning worker thread is to be stopped.
 *
 * We're unregistering any possible I/O watchers and timers, as used
 * by proxying connections and health checks.
 */
void Director::onStop()
{
	TRACE("onStop()");

	for (auto& br: backends_) {
		for (auto backend: br) {
			backend->disable();
			backend->healthMonitor().stop();
		}
	}
}

size_t Director::capacity() const
{
	size_t result = 0;

	for (auto& br: backends_)
		for (auto b: br)
			result += b->capacity();

	return result;
}

Backend* Director::createBackend(const std::string& name, const std::string& url)
{
	std::string protocol, hostname, path, query;
	int port = 0;

	if (!parseUrl(url, protocol, hostname, port, path, query)) {
		TRACE("invalid URL: %s", url.c_str());
		return false;
	}

	return createBackend(name, protocol, hostname, port, path, query);
}

Backend* Director::createBackend(const std::string& name, const std::string& protocol,
	const std::string& hostname, int port, const std::string& path, const std::string& query)
{
	int capacity = 1;

	// TODO createBackend<HttpBackend>(hostname, port);
	if (protocol == "http")
		return createBackend<HttpBackend>(name, capacity, hostname, port);

	return nullptr;
}

Backend* Director::findBackend(const std::string& name)
{
	for (auto& br: backends_)
		for (auto b: br)
			if (b->name() == name)
				return b;

	return nullptr;
}

/**
 * Schedules the passed request to the best fitting backend.
 * \param r the request to be processed by one of the directors backends.
 */
void Director::schedule(HttpRequest* r)
{
	r->responseHeaders.push_back("X-Director-Cluster", name_);

	r->setCustomData<DirectorNotes>(this);

	auto notes = r->customData<DirectorNotes>(this);

	bool allDisabled = false;

	if (notes->backend) {
		if (notes->backend->healthMonitor().isOnline()) {
			pass(r, notes, notes->backend);
		} else {
			// pre-selected a backend, but this one is not online, so generate a 503 to give the client some feedback
			r->log(Severity::error, "director: Requested backend '%s' is %s, and is unable to process requests.",
				notes->backend->name().c_str(), notes->backend->healthMonitor().state_str().c_str());
			r->status = x0::HttpError::ServiceUnavailable;
			r->finish();
		}
	}
	else if (Backend* backend = findLeastLoad(Backend::Role::Active, &allDisabled)) {
		pass(r, notes, backend);
	}
	else if (Backend* backend = findLeastLoad(Backend::Role::Standby, &allDisabled)) {
		pass(r, notes, backend);
	}
	else if (queue_.size() < queueLimit_ && !allDisabled) {
		enqueue(r);
	}
	else if (Backend* backend = findLeastLoad(Backend::Role::Backup)) {
		pass(r, notes, backend);
	}
	else if (queue_.size() < queueLimit_) {
		enqueue(r);
	}
	else {
		r->log(Severity::error, "director: '%s' queue limit %zu reached. Rejecting request.", name_.c_str(), queueLimit_);
		r->status = HttpError::ServiceUnavailable;
		r->finish();
	}
}

Backend* Director::findLeastLoad(Backend::Role role, bool* allDisabled)
{
	Backend* best = nullptr;
	size_t bestAvail = 0;
	size_t enabledAndOnline = 0;

	for (auto backend: backendsWith(role)) {
		if (!backend->isEnabled() || !backend->healthMonitor().isOnline()) {
			TRACE("findLeastLoad: skip %s (disabled)", backend->name().c_str());
			continue;
		}

		++enabledAndOnline;

		size_t l = backend->load().current();
		size_t c = backend->capacity();
		size_t avail = c - l;

#ifndef NDEBUG
		worker_->log(Severity::debug, "findLeastLoad: test %s (%zi/%zi, %zi)", backend->name().c_str(), l, c, avail);
#endif

		if (avail > bestAvail) {
#ifndef NDEBUG
			worker_->log(Severity::debug, " - select (%zi > %zi, %s, %s)", avail, bestAvail, backend->name().c_str(), 
					best ? best->name().c_str() : "(null)");
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
		worker_->log(Severity::debug, "selecting backend %s", best->name().c_str());
#endif
		return best;
	}

#ifndef NDEBUG
	worker_->log(Severity::debug, "selecting backend (role %d) failed", static_cast<int>(role));
#endif
	return nullptr;
}

void Director::pass(HttpRequest* r, DirectorNotes* notes, Backend* backend)
{
	notes->backend = backend;

	++load_;
	++backend->load_;

	backend->process(r);
}

void Director::link(Backend* backend)
{
	backends_[static_cast<size_t>(backend->role_)].push_back(backend);
}

void Director::unlink(Backend* backend)
{
	auto& br = backends_[static_cast<size_t>(backend->role_)];
	auto i = std::find(br.begin(), br.end(), backend);

	if (i != br.end()) {
		br.erase(i);
	}
}

bool Director::reschedule(HttpRequest* r, Backend* backend)
{
	auto notes = r->customData<DirectorNotes>(this);

	--backend->load_;
	notes->backend = nullptr;

	TRACE("requeue (retry-count: %zi / %zi)", notes->retryCount, maxRetryCount());

	if (notes->retryCount == maxRetryCount()) {
		--load_;

		r->status = HttpError::ServiceUnavailable;
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

	TRACE("requeue (retry-count: %zi / %zi): giving up", notes->retryCount, maxRetryCount());

	--load_;
	enqueue(r);

	return false;
}

/**
 * Enqueues given request onto the request queue.
 */
void Director::enqueue(HttpRequest* r)
{
	// direct delivery failed, due to overheated director. queueing then.

#ifndef NDEBUG
	r->log(Severity::debug, "Director %s overloaded. Queueing request.", name_.c_str());
#endif

	queue_.push_back(r);
	++queued_;
}

Backend* Director::nextBackend(Backend* backend, HttpRequest* r)
{
	auto& backends = backendsWith(backend->role());
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
void Director::release(Backend* backend)
{
	--load_;

	if (!backend->isTerminating())
		dequeueTo(backend);
	else
		backend->tryTermination();
}

HttpRequest* Director::dequeue()
{
	if (!queue_.empty()) {
		HttpRequest* r = queue_.front();
		queue_.pop_front();
		--queued_;
		return r;
	}

	return nullptr;
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the backend for serving.
 *
 * \param backend the backend to pass the dequeued request to.
 * \todo thread safety (queue_)
 */
void Director::dequeueTo(Backend* backend)
{
	if (HttpRequest* r = dequeue()) {
#ifndef NDEBUG
		r->log(Severity::debug, "Dequeueing request to backend %s", backend->name().c_str());
#endif
		r->connection.worker().post([backend, r]() { backend->assign(r); });
	}
}

void Director::writeJSON(Buffer& output)
{
	output << "\"" << name_ << "\": {\n"
		   << "  \"load\": " << load_ << ",\n"
		   << "  \"queued\": " << queued_ << ",\n"
		   << "  \"queue-limit\": " << queueLimit_ << ",\n"
		   << "  \"max-retry-count\": " << maxRetryCount_ << ",\n"
		   << "  \"mutable\": " << (isMutable() ? "true" : "false") << ",\n"
		   << "  \"members\": [";

	size_t backendNum = 0;

	for (auto& br: backends_) {
		for (auto backend: br) {
			if (backendNum++)
				output << ", ";

			output << "\n    {";
			backend->writeJSON(output);
			output << "}";
		}
	}

	output << "\n  ]\n}\n";
}

/**
 * Loads director configuration from given file.
 *
 * \param path The path to the file holding the configuration.
 *
 * \retval true Success.
 * \retval false Failed, detailed message in errno.
 */
bool Director::load(const std::string& path)
{
	std::ifstream in(path);

	while (in.good()) {
		char buf[4096];
		in.getline(buf, sizeof(buf));
		size_t len = in.gcount();

		if (!len || buf[0] == '#')
			continue;

		StringTokenizer st(buf, ",", '\\');
		std::vector<std::string> values(st.tokenize());

		if (values.size() < 8) {
			worker_->log(Severity::error, "director: Invalid record in director file.");
			continue;
		}

		std::string name(values[0]);
		std::string role(values[1]);
		size_t capacity = std::atoi(values[2].c_str());
		std::string protocol(values[3]);
		bool enabled = values[4] == "true";
		std::string transport(values[5]);
		std::string hostname(values[6]);
		int port = std::atoi(values[7].c_str());

		Backend* backend = new HttpBackend(this, name, capacity, hostname, port);
		if (!enabled)
			backend->disable();
		if (role == "active")
			backend->setRole(Backend::Role::Active);
		else if (role == "standby")
			backend->setRole(Backend::Role::Standby);
		else if (role == "backup")
			backend->setRole(Backend::Role::Backup);
		else
			worker_->log(Severity::error, "Invalid backend role '%s'", role.c_str());
	}

	setMutable(true);

	return true;
}

/**
 * stores director configuration in a plaintext file.
 *
 * \param pathOverride if not empty, store data into this file's path, oetherwise use the one we loaded from.
 * \todo this must happen asynchronousely, never ever block within the callers thread (or block in a dedicated thread).
 */
bool Director::store(const std::string& pathOverride)
{
	static const std::string header("name,role,capacity,protocol,enabled,transport,host,port");
	std::string path = !pathOverride.empty() ? pathOverride : storagePath_;
	std::ofstream out(path, std::ios_base::out | std::ios_base::trunc);

	// TODO

	return true;
}
