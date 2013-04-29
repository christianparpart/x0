/* <plugins/director/Director.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "Director.h"
#include "Backend.h"
#include "HttpBackend.h"
#include "RequestNotes.h"
#include "FastCgiBackend.h"

#include <x0/io/BufferSource.h>
#include <x0/Tokenizer.h>
#include <x0/IniFile.h>
#include <x0/Url.h>
#include <fstream>

#if 1 // !defined(XZERO_NDEBUG)
#	define TRACE(level, msg...) worker_->log(Severity::debug ## level, "director: " msg)
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

struct BackendData : public CustomData
{
	BackendRole role;

	BackendData() :
		role()
	{
	}

	virtual ~BackendData()
	{
	}
};

static inline const std::string& role2str(BackendRole role)
{
	static std::string map[] = {
		"active",
		"standby",
		"backup",
		"terminate",
	};
	return map[static_cast<size_t>(role)];
}

/**
 * Initializes a director object (load balancer instance).
 *
 * \param worker the worker associated with this director's local jobs (e.g. backend health checks).
 * \param name the unique human readable name of this director (load balancer) instance.
 */
Director::Director(HttpWorker* worker, const std::string& name) :
	BackendManager(worker, name),
	mutable_(false),
	healthCheckHostHeader_("backend-healthcheck"),
	healthCheckRequestPath_("/"),
	healthCheckFcgiScriptFilename_(),
	stickyOfflineMode_(false),
	backends_(),
	queueLimit_(128),
	queueTimeout_(TimeSpan::fromSeconds(60)),
	retryAfter_(TimeSpan::fromSeconds(10)),
	maxRetryCount_(6),
	storagePath_(),
	shaper_(worker->loop(), 0),
	load_(),
	queued_(),
	dropped_(0),
	stopHandle_()
{
	backends_.resize(3);

	stopHandle_ = worker->registerStopHandler(std::bind(&Director::onStop, this));

	shaper_.setTimeoutHandler(std::bind(&Director::onTimeout, this, std::placeholders::_1));
}

Director::~Director()
{
	worker()->unregisterStopHandler(stopHandle_);

	for (auto& backendRole: backends_) {
		while (!backendRole.empty()) {
			delete unlink(backendRole.back());
		}
	}
}

/** Callback, that updates shaper capacity based on enabled/health state.
 *
 * This callback gets invoked when a backend's enabled state toggled between
 * the values \c true and \c false.
 * It will then try to either increase the shaping capacity or reduce it.
 *
 * \see onBackendStateChanged
 */
void Director::onBackendEnabledChanged(const Backend* backend)
{
	TRACE(1, "onBackendEnabledChanged: health=%s, enabled=%s",
			stringify(backend->healthMonitor()->state()).c_str(),
			backend->isEnabled() ? "true" : "false");

	if (backendRole(backend) != BackendRole::Active)
		return;

	if (backend->healthMonitor()->isOnline()) {
		if (backend->isEnabled()) {
			TRACE(1, "onBackendEnabledChanged: adding capacity to shaper (%zi + %zi)",
					shaper()->size(), backend->capacity());
			shaper()->resize(shaper()->size() + backend->capacity());
		} else {
			TRACE(1, "onBackendEnabledChanged: removing capacity from shaper (%zi - %zi)",
					shaper()->size(), backend->capacity());
			shaper()->resize(shaper()->size() - backend->capacity());
		}
	}
}

void Director::onBackendStateChanged(Backend* backend, HealthMonitor* healthMonitor, HealthState oldState)
{
	TRACE(1, "onBackendStateChanged: health=%s -> %s, enabled=%s",
			stringify(oldState).c_str(),
			stringify(backend->healthMonitor()->state()).c_str(),
			backend->isEnabled() ? "true" : "false");

	worker_->log(Severity::info, "Director '%s': backend '%s' is now %s.",
		name().c_str(), backend->name_.c_str(), healthMonitor->state_str().c_str());

	if (healthMonitor->isOnline()) {
		if (!backend->isEnabled())
			return;

		// backend is online and enabled

		TRACE(1, "onBackendStateChanged: adding capacity to shaper (%zi + %zi)", shaper()->size(), backend->capacity());
		shaper()->resize(shaper()->size() + backend->capacity());

		if (!stickyOfflineMode()) {
			// try delivering a queued request
			dequeueTo(backend);
		} else {
			// disable backend due to sticky-offline mode
			worker_->log(Severity::notice, "Director '%s': backend '%s' disabled due to sticky offline mode.",
				name().c_str(), backend->name().c_str());
			backend->setEnabled(false);
		}
	} else if (backend->isEnabled() && oldState == HealthState::Online) {
		// backend is offline and enabled
		shaper()->resize(shaper()->size() - backend->capacity());
		TRACE(1, "onBackendStateChanged: removing capacity from shaper (%zi - %zi)", shaper()->size(), backend->capacity());
	}
}

/** The currently associated backend has rejected processing the given request,
 *  so put it back to the cluster try rescheduling it to another backend.
 */
void Director::reject(x0::HttpRequest* r)
{
	auto notes = requestNotes(r);
	reschedule(r, notes);
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
 * \see Backend::release()
 */
void Director::release(Backend* backend, HttpRequest* r)
{
	--load_;

	// explicitely clear reuqest notes here, so we also free up its acquired shaper tokens
	r->clearCustomData(this);

	if (backendRole(backend) != BackendRole::Terminate) {
		dequeueTo(backend);
	} else if (backend->load().current() == 0) {
		backend->healthMonitor()->stop();
		delete backend;
		save();
	}
}

/**
 * Callback, invoked when the owning worker thread is to be stopped.
 *
 * We're unregistering any possible I/O watchers and timers, as used
 * by proxying connections and health checks.
 */
void Director::onStop()
{
	TRACE(1, "onStop()");

	for (auto& br: backends_) {
		for (auto backend: br) {
			backend->disable();
			backend->healthMonitor()->stop();
		}
	}
}

size_t Director::capacity() const
{
	size_t result = 0;

	for (const auto& br: backends_)
		result += br.capacity();

	return result;
}

x0::TokenShaperError Director::createBucket(const std::string& name, float rate, float ceil)
{
	return shaper_.createNode(name, rate, ceil);
}

RequestShaper::Node* Director::findBucket(const std::string& name) const
{
	return shaper_.findNode(name);
}

bool Director::eachBucket(std::function<bool(RequestShaper::Node*)> body)
{
	for (auto& node: *shaper_.rootNode())
		if (!body(node))
			return false;

	return true;
}

RequestNotes* Director::setupRequestNotes(HttpRequest* r, Backend* backend)
{
	return r->setCustomData<RequestNotes>(this, r->connection.worker().now(), backend);
}

RequestNotes* Director::requestNotes(HttpRequest* r)
{
	return r->customData<RequestNotes>(this);
}

Backend* Director::createBackend(const std::string& name, const Url& url)
{
	SocketSpec spec = SocketSpec::fromInet(IPAddress(url.hostname()), url.port());
	int capacity = 1;
	BackendRole role = BackendRole::Active;

	return createBackend(name, url.protocol(), spec, capacity, role);
}

Backend* Director::createBackend(const std::string& name, const std::string& protocol, const x0::SocketSpec& socketSpec, size_t capacity, BackendRole role)
{
	if (findBackend(name))
		return nullptr;

	Backend* backend;

	if (protocol == "fastcgi") {
		backend = new FastCgiBackend(this, name, socketSpec, capacity, true);
	} else if (protocol == "http") {
		backend = new HttpBackend(this, name, socketSpec, capacity, true);
	} else {
		return nullptr;
	}

	backend->disable(); // ensure backend is disabled upon creation

	BackendData* bd = backend->setCustomData<BackendData>(this);
	bd->role = role;

	link(backend, role);

	backend->setEnabledCallback([this](const Backend* backend) {
		onBackendEnabledChanged(backend);
	});

	backend->healthMonitor()->setStateChangeCallback([this, backend](HealthMonitor*, HealthState oldState) {
		onBackendStateChanged(backend, backend->healthMonitor(), oldState);
	});

	backend->setJsonWriteCallback([role](const Backend*, JsonWriter& json) {
		json.name("role")(role2str(role));
	});

	// wake up the worker's event loop here, so he knows in time about the health check timer we just installed.
	// TODO we should not need this...
	worker()->wakeup();

	return backend;
}

void Director::terminateBackend(Backend* backend)
{
	setBackendRole(backend, BackendRole::Terminate);
}

void Director::link(Backend* backend, BackendRole role)
{
	BackendData* data = backend->customData<BackendData>(this);
	data->role = role;

	backends_[static_cast<size_t>(role)].push_back(backend);
}

Backend* Director::unlink(Backend* backend)
{
	auto& br = backends_[static_cast<size_t>(backendRole(backend))];
	br.remove(backend);
	return backend;
}

BackendRole Director::backendRole(const Backend* backend) const
{
	return backend->customData<BackendData>(this)->role;
}

Backend* Director::findBackend(const std::string& name)
{
	for (auto& br: backends_)
		for (auto b: br)
			if (b->name() == name)
				return b;

	return nullptr;
}

void Director::setBackendRole(Backend* backend, BackendRole role)
{
	BackendRole currentRole = backendRole(backend);
	worker_->log(Severity::debug, "setBackendRole(%d) (from %d)", role, currentRole);

	if (role != currentRole) {
		if (role == BackendRole::Active)
			shaper()->resize(shaper()->size() + backend->capacity());
		else
			shaper()->resize(shaper()->size() - backend->capacity());

		if (role == BackendRole::Terminate) {
			unlink(backend);

			if (backend->load().current()) {
				link(backend, role);
			} else {
				delete backend;
				save();
			}
		} else {
			unlink(backend);
			link(backend, role);
		}
	}
}

void Director::writeJSON(JsonWriter& json) const
{
	json.beginObject()
		.name("mutable")(isMutable())
		.name("load")(load_)
		.name("queued")(queued_)
		.name("dropped")(dropped_)
		.name("queue-limit")(queueLimit_)
		.name("queue-timeout")(queueTimeout_.totalMilliseconds())
		.name("retry-after")(retryAfter_.totalSeconds())
		.name("max-retry-count")(maxRetryCount_)
		.name("sticky-offline-mode")(stickyOfflineMode_)
		.name("connect-timeout")(connectTimeout_.totalMilliseconds())
		.name("read-timeout")(readTimeout_.totalMilliseconds())
		.name("write-timeout")(writeTimeout_.totalMilliseconds())
		.name("transfer-mode")(transferMode_)
		.name("health-check-host-header")(healthCheckHostHeader_)
		.name("health-check-request-path")(healthCheckRequestPath_)
		.name("health-check-fcgi-script-name")(healthCheckFcgiScriptFilename_)
		.name("shaper")(shaper_)
		.beginArray("members");

	for (auto& br: backends_) {
		for (auto& backend: br) {
			json.value(*backend);
		}
	}

	json.endArray();
	json.endObject();
}

namespace x0 {
	x0::JsonWriter& operator<<(x0::JsonWriter& json, const Director& director)
	{
		director.writeJSON(json);
		return json;
	}
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
	// treat director as loaded if db file behind given path does not exist
	struct stat st;
	if (stat(path.c_str(), &st) < 0 && errno == ENOENT) {
		storagePath_ = path;
		setMutable(true);
		return save();
	}

	storagePath_ = path;

	IniFile settings;
	if (!settings.loadFile(path)) {
		worker()->log(Severity::error, "director: Could not load director settings from file '%s'. %s", path.c_str(), strerror(errno));
		return false;
	}

	std::string value;
	if (!settings.load("director", "queue-limit", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.queue-limit in file '%s'", path.c_str());
		return false;
	}
	queueLimit_ = std::atoll(value.c_str());

	if (!settings.load("director", "queue-timeout", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.queue-timeout in file '%s'", path.c_str());
		return false;
	}
	queueTimeout_ = TimeSpan::fromMilliseconds(std::atoll(value.c_str()));

	if (!settings.load("director", "retry-after", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.retry-after in file '%s'", path.c_str());
		return false;
	}
	retryAfter_ = TimeSpan::fromSeconds(std::atoll(value.c_str()));

	if (!settings.load("director", "connect-timeout", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.connect-timeout in file '%s'", path.c_str());
		return false;
	}
	connectTimeout_ = TimeSpan::fromMilliseconds(std::atoll(value.c_str()));

	if (!settings.load("director", "read-timeout", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.read-timeout in file '%s'", path.c_str());
		return false;
	}
	readTimeout_ = TimeSpan::fromMilliseconds(std::atoll(value.c_str()));

	if (!settings.load("director", "write-timeout", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.write-timeout in file '%s'", path.c_str());
		return false;
	}
	writeTimeout_ = TimeSpan::fromMilliseconds(std::atoll(value.c_str()));

	if (!settings.load("director", "transfer-mode", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.transfer-mode in file '%s'. Defaulting to 'blocking'.", path.c_str());
		value = "blocking";
	}
	transferMode_ = makeTransferMode(value);

	if (!settings.load("director", "max-retry-count", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.max-retry-count in file '%s'", path.c_str());
		return false;
	}
	maxRetryCount_ = std::atoll(value.c_str());

	if (!settings.load("director", "sticky-offline-mode", value)) {
		worker()->log(Severity::error, "director: Could not load settings value director.sticky-offline-mode in file '%s'", path.c_str());
		return false;
	}
	stickyOfflineMode_ = value == "true";

	if (!settings.load("director", "health-check-host-header", healthCheckHostHeader_)) {
		worker()->log(Severity::error, "director: Could not load settings value director.health-check-host-header in file '%s'", path.c_str());
		return false;
	}

	if (!settings.load("director", "health-check-request-path", healthCheckRequestPath_)) {
		worker()->log(Severity::error, "director: Could not load settings value director.health-check-request-path in file '%s'", path.c_str());
		return false;
	}

	if (!settings.load("director", "health-check-fcgi-script-filename", healthCheckFcgiScriptFilename_)) {
		healthCheckFcgiScriptFilename_ = "";
	}

	for (auto& section: settings) {
		static const std::string backendSectionPrefix("backend=");

		std::string key = section.first;
		if (key == "director")
			continue;

		bool result = false;
		if (key.find(backendSectionPrefix) == 0)
			result = loadBackend(settings, key);
		else if (key.find("bucket=") == 0)
			result = loadBucket(settings, key);
		else {
			worker()->log(Severity::error, "director: Invalid configuration section '%s' in file '%s'.", key.c_str(), path.c_str());
			result = false;
		}

		if (!result) {
			return false;
		}
	}

	setMutable(true);

	return true;
}

bool Director::loadBucket(const IniFile& settings, const std::string& key)
{
	std::string name = key.substr(strlen("bucket="));

	std::string rateStr;
	if (!settings.load(key, "rate", rateStr)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'rate' not found in section '%s'.", storagePath_.c_str(), key.c_str());
		return false;
	}

	std::string ceilStr;
	if (!settings.load(key, "ceil", ceilStr)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'ceil' not found in section '%s'.", storagePath_.c_str(), key.c_str());
		return false;
	}

	char* nptr = nullptr;
	float rate = strtof(rateStr.c_str(), &nptr);
	float ceil = strtof(ceilStr.c_str(), &nptr);


	TokenShaperError ec = createBucket(name, rate, ceil);
	if (ec != TokenShaperError::Success) {
		static const char *str[] = {
			"Success.",
			"Rate limit overflow.",
			"Ceil limit overflow.",
			"Name conflict.",
			"Invalid child node.",
		};
		worker()->log(Severity::error, "Could not create director's bucket. %s", str[(size_t)ec]);
		return false;
	}

	return true;
}

bool Director::loadBackend(const IniFile& settings, const std::string& key)
{
	std::string name = key.substr(strlen("backend="));

	// role
	std::string roleStr;
	if (!settings.load(key, "role", roleStr)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'role' not found in section '%s'.", storagePath_.c_str(), key.c_str());
		return false;
	}

	BackendRole role = BackendRole::Terminate; // aka. Undefined
	if (roleStr == "active")
		role = BackendRole::Active;
	else if (roleStr == "backup")
		role = BackendRole::Backup;
	else {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'role' for backend '%s' contains invalid data '%s'.", storagePath_.c_str(), key.c_str(), roleStr.c_str());
		return false;
	}

	// capacity
	std::string capacityStr;
	if (!settings.load(key, "capacity", capacityStr)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'capacity' not found in section '%s'.", storagePath_.c_str(), key.c_str());
		return false;
	}
	size_t capacity = std::atoll(capacityStr.c_str());

	// protocol
	std::string protocol;
	if (!settings.load(key, "protocol", protocol)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'protocol' not found in section '%s'.", storagePath_.c_str(), key.c_str());
		return false;
	}

	// enabled
	std::string enabledStr;
	if (!settings.load(key, "enabled", enabledStr)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'enabled' not found in section '%s'.", storagePath_.c_str(), key.c_str());
		return false;
	}
	bool enabled = enabledStr == "true";

	// health-check-interval
	std::string hcIntervalStr;
	if (!settings.load(key, "health-check-interval", hcIntervalStr)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'health-check-interval' not found in section '%s'.", storagePath_.c_str(), key.c_str());
		return false;
	}
	TimeSpan hcInterval = TimeSpan::fromMilliseconds(std::atoll(hcIntervalStr.c_str()));

	// health-check-mode
	std::string hcModeStr;
	if (!settings.load(key, "health-check-mode", hcModeStr)) {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'health-check-mode' not found in section '%s'.", storagePath_.c_str(), hcModeStr.c_str(), key.c_str());
		return false;
	}

	HealthMonitor::Mode hcMode;
	if (hcModeStr == "paranoid")
		hcMode = HealthMonitor::Mode::Paranoid;
	else if (hcModeStr == "opportunistic")
		hcMode = HealthMonitor::Mode::Opportunistic;
	else if (hcModeStr == "lazy")
		hcMode = HealthMonitor::Mode::Lazy;
	else {
		worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'health-check-mode' invalid ('%s') in section '%s'.", storagePath_.c_str(), hcModeStr.c_str(), key.c_str());
		return false;
	}

	SocketSpec socketSpec;
	std::string path;
	if (settings.load(key, "path", path)) {
		socketSpec = SocketSpec::fromLocal(path);
	} else {
		// host
		std::string host;
		if (!settings.load(key, "host", host)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'host' not found in section '%s'.", storagePath_.c_str(), key.c_str());
			return false;
		}

		// port
		std::string portStr;
		if (!settings.load(key, "port", portStr)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'port' not found in section '%s'.", storagePath_.c_str(), key.c_str());
			return false;
		}

		int port = std::atoi(portStr.c_str());
		if (port <= 0) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Invalid port number '%s' for backend '%s'", storagePath_.c_str(), portStr.c_str(), name.c_str());
			return false;
		}

		socketSpec = SocketSpec::fromInet(IPAddress(host), port);
	}

	// spawn backend (by protocol)
	Backend* backend = createBackend(name, protocol, socketSpec, capacity, role);
	if (!backend) {
		worker()->log(Severity::error, "director: Invalid protocol '%s' for backend '%s' in configuration file '%s'.", protocol.c_str(), name.c_str(), storagePath_.c_str());
		return false;
	}

	backend->setEnabled(enabled);
	backend->healthMonitor()->setMode(hcMode);
	backend->healthMonitor()->setInterval(hcInterval);

	return true;
}

/**
 * stores director configuration in a plaintext file.
 *
 * \todo this must happen asynchronousely, never ever block within the callers thread (or block in a dedicated thread).
 */
bool Director::save()
{
	static const std::string header("name,role,capacity,protocol,enabled,transport,host,port");
	std::string path = storagePath_;
	std::ofstream out(path, std::ios_base::out | std::ios_base::trunc);

	out << "# vim:syntax=dosini\n"
		<< "# !!! DO NOT EDIT !!! THIS FILE IS GENERATED AUTOMATICALLY !!!\n\n"
		<< "[director]\n"
		<< "queue-limit=" << queueLimit_ << "\n"
		<< "queue-timeout=" << queueTimeout_.totalMilliseconds() << "\n"
		<< "retry-after=" << retryAfter_.totalSeconds() << "\n"
		<< "max-retry-count=" << maxRetryCount_ << "\n"
		<< "sticky-offline-mode=" << (stickyOfflineMode_ ? "true" : "false") << "\n"
		<< "connect-timeout=" << connectTimeout_.totalMilliseconds() << "\n"
		<< "read-timeout=" << readTimeout_.totalMilliseconds() << "\n"
		<< "write-timeout=" << writeTimeout_.totalMilliseconds() << "\n"
		<< "health-check-host-header=" << healthCheckHostHeader_ << "\n"
		<< "health-check-request-path=" << healthCheckRequestPath_ << "\n"
		<< "health-check-fcgi-script-filename=" << healthCheckFcgiScriptFilename_ << "\n"
		<< "\n";

	for (auto& bucket: *shaper()->rootNode()) {
		out << "[bucket=" << bucket->name() << "]\n"
			<< "rate=" << bucket->rate() << "\n"
			<< "ceil=" << bucket->ceil() << "\n"
			<< "\n";
	}

	for (auto& br: backends_) {
		for (auto b: br) {
			out << "[backend=" << b->name() << "]\n"
				<< "role=" << role2str(backendRole(b)) << "\n"
				<< "capacity=" << b->capacity() << "\n"
				<< "enabled=" << (b->isEnabled() ? "true" : "false") << "\n"
				<< "transport=" << (b->socketSpec().isLocal() ? "local" : "tcp") << "\n"
				<< "protocol=" << b->protocol() << "\n"
				<< "health-check-mode=" << b->healthMonitor()->mode_str() << "\n"
				<< "health-check-interval=" << b->healthMonitor()->interval().totalMilliseconds() << "\n";

			if (b->socketSpec().isInet()) {
				out << "host=" << b->socketSpec().ipaddr().str() << "\n";
				out << "port=" << b->socketSpec().port() << "\n";
			} else {
				out << "path=" << b->socketSpec().local() << "\n";
			}

			out << "\n";
		}
	}

	return true;
}

/**
 * Schedules a new request to be directly processed by a given backend.
 *
 * This has not much to do with scheduling, especially where the target backend
 * has been already chosen. This target backend must be used or an error
 * has to be served, e.g. when this backend is offline, disabled, or overloarded.
 *
 * This request will be attempted to be scheduled on this backend only once.
 */
void Director::schedule(HttpRequest* r, Backend* backend)
{
	auto notes = setupRequestNotes(r);
	notes->backend = backend;
	notes->bucket = shaper()->rootNode();

	r->responseHeaders.push_back("X-Director-Bucket", notes->bucket->name());

	if (!notes->bucket->get()) {
		tryEnqueue(r, notes);
		return;
	}

	switch (backend->tryProcess(r)) {
	case SchedulerStatus::Unavailable:
	case SchedulerStatus::Overloaded:
		r->log(Severity::error, "director: Requested backend '%s' is %s, and is unable to process requests.",
			backend->name().c_str(), backend->healthMonitor()->state_str().c_str());
		serviceUnavailable(r);

		// TODO: consider backend-level queues as a feature here (post 0.7 release)
		break;
	case SchedulerStatus::Success:
		break;
	}
}

/**
 * Schedules a new request via the given bucket on this cluster.
 *
 * This method will attempt to process the request on any of the available
 * backends if and only if the chosen bucket has enough resources currently available.
 *
 * If the bucket does not allow processing further requests, due to its ceiling limits,
 * the request must be enqueued in the bucket's local queue.
 *
 * If the queue has reached the queue limit already, a 503 (Service Unavailable) will be
 * responded instead.
 */
void Director::schedule(HttpRequest* r, RequestShaper::Node* bucket)
{
	auto notes = setupRequestNotes(r);
	notes->bucket = bucket;

	r->responseHeaders.push_back("X-Director-Bucket", bucket->name());

	if (notes->bucket->get()) {
		notes->tokens = 1;
		SchedulerStatus result1 = tryProcess(r, notes, BackendRole::Active);
		if (result1 == SchedulerStatus::Success)
			return;

		if (result1 == SchedulerStatus::Unavailable &&
				tryProcess(r, notes, BackendRole::Backup) == SchedulerStatus::Success)
			return;

		// we could not actually processes the request, so release the token we just received.
		notes->bucket->put();
	}

	tryEnqueue(r, notes);
}

/**
 * Verifies number of tries, this request has been attempted to be queued, to be in valid range.
 *
 * \retval true tryCount is still below threashold, so further tries are allowed.
 * \retval false tryCount exceeded limit and a 503 client response has been sent. Dropped-stats have been incremented.
 */
bool Director::verifyTryCount(HttpRequest* r, RequestNotes* notes)
{
	if (notes->tryCount <= maxRetryCount())
		return true;

	r->log(Severity::info, "director: %s request failed %d times. Dropping.", name().c_str(), notes->tryCount);
	serviceUnavailable(r);
	++dropped_;
	return false;
}

void Director::reschedule(HttpRequest* r, RequestNotes* notes)
{
	if (!verifyTryCount(r, notes))
		return;

	SchedulerStatus result1 = tryProcess(r, notes, BackendRole::Active);
	if (result1 == SchedulerStatus::Success)
		return;

	if (result1 == SchedulerStatus::Unavailable &&
			tryProcess(r, notes, BackendRole::Backup) == SchedulerStatus::Success)
		return;

	tryEnqueue(r, notes);
}

void Director::schedule(HttpRequest* r, RequestNotes* notes)
{
	if (notes->tryCount == 0) {
		r->responseHeaders.push_back("X-Director-Cluster", name());

		if (notes->backend) {
			// pre-selected a backend
			switch (notes->backend->tryProcess(r)) {
			case SchedulerStatus::Unavailable:
			case SchedulerStatus::Overloaded:
				r->log(Severity::error, "director: Requested backend '%s' is %s, and is unable to process requests.",
					notes->backend->name().c_str(), notes->backend->healthMonitor()->state_str().c_str());
				serviceUnavailable(r);
				break;
			case SchedulerStatus::Success:
				break;
			}
			return;
		}
	} else {
		notes->backend = nullptr;

		if (notes->tryCount > maxRetryCount()) {
			r->log(Severity::info, "director: %s request failed %d times. Dropping.", name().c_str(), notes->tryCount);
			serviceUnavailable(r);
			++dropped_;
			return;
		}
	}

	SchedulerStatus result1 = tryProcess(r, notes, BackendRole::Active);
	if (result1 == SchedulerStatus::Success)
		return;

	if (result1 == SchedulerStatus::Unavailable &&
			tryProcess(r, notes, BackendRole::Backup) == SchedulerStatus::Success)
		return;

	tryEnqueue(r, notes);
}

/**
 * Finishes a request with a 503 (Service Unavailable) response message.
 */
void Director::serviceUnavailable(HttpRequest* r)
{
	r->status = HttpStatus::ServiceUnavailable;

	if (retryAfter()) {
		char value[64];
		snprintf(value, sizeof(value), "%zu", retryAfter().totalSeconds());
		r->responseHeaders.push_back("Retry-After", value);
	}

	r->finish();
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the backend for serving.
 *
 * \param backend the backend to pass the dequeued request to.
 */
void Director::dequeueTo(Backend* backend)
{
	if (HttpRequest* r = dequeue()) {
		r->post([this, backend, r]() {
			auto notes = requestNotes(r);
			notes->tokens = 1;
#ifndef XZERO_NDEBUG
			r->log(Severity::debug, "Dequeueing request to backend %s @ %s",
				backend->name().c_str(), name().c_str());
#endif
			if (tryProcess(r, notes, backend) != SchedulerStatus::Success) {
				r->log(Severity::error, "Dequeueing request to backend %s @ %s failed.", backend->name().c_str(), name().c_str());
				schedule(r, notes);
			} else {
				verifyTryCount(r, notes);
			}
		});
	} else {
		log(LogMessage(Severity::debug, "dequeueTo: queue empty."));
	}
}

/**
 * Attempts to enqueue the request, respecting limits.
 *
 * Attempts to enqueue the request on the associated bucket.
 * If enqueuing fails, it instead finishes the request with a 503 (Service Unavailable).
 *
 * \retval true request could be enqueued.
 * \retval false request could not be enqueued. A 503 error response has been sent out instead.
 */
bool Director::tryEnqueue(HttpRequest* r, RequestNotes* notes)
{
	if (notes->bucket->queued().current() < queueLimit()) {
		notes->bucket->enqueue(r);
		notes->tokens = 0;
		++queued_;

		r->log(Severity::info, "Director %s [%s] overloaded. Enqueueing request (%d).",
			name().c_str(), notes->bucket->name().c_str(), notes->bucket->queued().current());

		return true;
	}

	r->log(Severity::info, "director: '%s' queue limit %zu reached. Rejecting request.", name().c_str(), queueLimit());
	serviceUnavailable(r);
	++dropped_;

	return false;
}

HttpRequest* Director::dequeue()
{
	if (HttpRequest* r = shaper()->dequeue()) {
		--queued_;
		r->log(Severity::debug, "Director %s dequeued request (%d left).", name().c_str(), queued_.current());
		return r;
	}

	return nullptr;
}

#ifndef XZERO_NDEBUG
inline const char* roleStr(BackendRole role)
{
	switch (role) {
		case BackendRole::Active: return "Active";
		case BackendRole::Backup: return "Backup";
		case BackendRole::Terminate: return "Terminate";
		default: return "UNKNOWN";
	}
}
#endif

SchedulerStatus Director::tryProcess(x0::HttpRequest* r, RequestNotes* notes, BackendRole role)
{
	++notes->tryCount;
	++load_;

	SchedulerStatus result = backends_[static_cast<size_t>(role)].schedule(r);

	if (result != SchedulerStatus::Success)
		--load_;

	return result;
}

/**
 * Attempts to process request on given backend.
 */
SchedulerStatus Director::tryProcess(HttpRequest* r, RequestNotes* notes, Backend* backend)
{
	notes->backend = backend;
	++notes->tryCount;

	++load_;

	SchedulerStatus result = backend->tryProcess(r);

	if (result != SchedulerStatus::Success)
		--load_;

	return result;
}

void Director::onTimeout(HttpRequest* r)
{
	--queued_;
	++dropped_;

	r->post([this, r]() {
		r->log(Severity::info, "Queued request timed out. Dropping. %s %s", r->method.str().c_str(), r->unparsedUri.str().c_str());
		serviceUnavailable(r);
	});
}
