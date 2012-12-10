/* <plugins/director/Director.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "Director.h"
#include "Backend.h"
#include "HttpBackend.h"
#include "FastCgiBackend.h"
#include "LeastLoadScheduler.h"
#include "ClassfulScheduler.h"

#include <x0/io/BufferSource.h>
#include <x0/StringTokenizer.h>
#include <x0/IniFile.h>
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
	scheduler_(nullptr)
{
	backends_.resize(4);

	stopHandle_ = worker->registerStopHandler(std::bind(&Director::onStop, this));

	scheduler_ = new LeastLoadScheduler(this);
	//scheduler_ = new ClassfulScheduler(this);
}

Director::~Director()
{
	worker()->unregisterStopHandler(stopHandle_);

	for (auto backendRoles: backends_) {
		for (auto backend: backendRoles) {
			unlink(backend);
			delete backend;
		}
	}

	delete scheduler_;
}

/** The currently associated backend has rejected processing the given request,
 *  so put it back to the cluster try rescheduling it to another backend.
 */
void Director::reject(x0::HttpRequest* r)
{
	scheduler_->schedule(r);
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
			backend->healthMonitor()->stop();
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
	int capacity = 1;
	Backend* backend;

	if (url.protocol() == "http")
		backend = new HttpBackend(this, name,
			SocketSpec::fromInet(IPAddress(url.hostname()), url.port()), capacity);
	else if (url.protocol() == "fcgi" || url.protocol() == "fastcgi")
		backend = new FastCgiBackend(this, name,
			SocketSpec::fromInet(IPAddress(url.hostname()), url.port()), capacity);
	else
		return nullptr;

	link(backend);

	// wake up the worker's event loop here, so he knows in time about the health check timer we just installed.
	// TODO we should not need this...
	worker()->wakeup();

	return backend;
}

void Director::destroyBackend(Backend* backend)
{
	unlink(backend);
	save();
	delete backend;
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

Backend* Director::findBackend(const std::string& name)
{
	for (auto& br: backends_)
		for (auto b: br)
			if (b->name() == name)
				return b;

	return nullptr;
}

void Director::writeJSON(JsonWriter& json) const
{
	json.beginObject()
		.name("mutable")(isMutable())
		.name("queue-limit")(queueLimit_)
		.name("queue-timeout")(queueTimeout_.totalMilliseconds())
		.name("retry-after")(retryAfter_.totalSeconds())
		.name("max-retry-count")(maxRetryCount_)
		.name("sticky-offline-mode")(stickyOfflineMode_)
		.name("connect-timeout")(connectTimeout_.totalMilliseconds())
		.name("read-timeout")(readTimeout_.totalMilliseconds())
		.name("write-timeout")(writeTimeout_.totalMilliseconds())
		.name("health-check-host-header")(healthCheckHostHeader_)
		.name("health-check-request-path")(healthCheckRequestPath_)
		.name("health-check-fcgi-script-name")(healthCheckFcgiScriptFilename_)
		.name("scheduler")(*scheduler_)
		.beginArray("members");

	for (auto& br: backends_) {
		for (auto backend: br) {
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

		if (key.find(backendSectionPrefix) != 0) {
			worker()->log(Severity::error, "director: Invalid configuration section '%s' in file '%s'.", key.c_str(), path.c_str());
			return false;
		}

		std::string name = key.substr(backendSectionPrefix.size());

		// role
		std::string roleStr;
		if (!settings.load(key, "role", roleStr)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'role' not found in section '%s'.", path.c_str(), key.c_str());
			return false;
		}

		Backend::Role role = Backend::Role::Terminate; // aka. Undefined
		if (roleStr == "active")
			role = Backend::Role::Active;
		else if (roleStr == "standby")
			role = Backend::Role::Standby;
		else if (roleStr == "backup")
			role = Backend::Role::Backup;
		else {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'role' for backend '%s' contains invalid data '%s'.", path.c_str(), key.c_str(), roleStr.c_str());
			return false;
		}

		// capacity
		std::string capacityStr;
		if (!settings.load(key, "capacity", capacityStr)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'capacity' not found in section '%s'.", path.c_str(), key.c_str());
			return false;
		}
		size_t capacity = std::atoll(capacityStr.c_str());

		// protocol
		std::string protocol;
		if (!settings.load(key, "protocol", protocol)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'protocol' not found in section '%s'.", path.c_str(), key.c_str());
			return false;
		}

		// enabled
		std::string enabledStr;
		if (!settings.load(key, "enabled", enabledStr)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'enabled' not found in section '%s'.", path.c_str(), key.c_str());
			return false;
		}
		bool enabled = enabledStr == "true";

		// health-check-interval
		std::string hcIntervalStr;
		if (!settings.load(key, "health-check-interval", hcIntervalStr)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'health-check-interval' not found in section '%s'.", path.c_str(), key.c_str());
			return false;
		}
		TimeSpan hcInterval = TimeSpan::fromMilliseconds(std::atoll(hcIntervalStr.c_str()));

		// health-check-mode
		std::string hcModeStr;
		if (!settings.load(key, "health-check-mode", hcModeStr)) {
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'health-check-mode' not found in section '%s'.", path.c_str(), hcModeStr.c_str(), key.c_str());
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
			worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'health-check-mode' invalid ('%s') in section '%s'.", path.c_str(), hcModeStr.c_str(), key.c_str());
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
				worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'host' not found in section '%s'.", path.c_str(), key.c_str());
				return false;
			}

			// port
			std::string portStr;
			if (!settings.load(key, "port", portStr)) {
				worker()->log(Severity::error, "director: Error loading configuration file '%s'. Item 'port' not found in section '%s'.", path.c_str(), key.c_str());
				return false;
			}

			int port = std::atoi(portStr.c_str());
			if (port <= 0) {
				worker()->log(Severity::error, "director: Error loading configuration file '%s'. Invalid port number '%s' for backend '%s'", path.c_str(), portStr.c_str(), name.c_str());
				return false;
			}

			socketSpec = SocketSpec::fromInet(IPAddress(host), port);
		}

		// spawn backend (by protocol)
		Backend* backend = nullptr;
		if (protocol == "fastcgi") {
			backend = new FastCgiBackend(this, name, socketSpec, capacity);
		} else if (protocol == "http") {
			backend = new HttpBackend(this, name, socketSpec, capacity);
		} else {
			worker()->log(Severity::error, "director: Invalid protocol '%s' for backend '%s' in configuration file '%s'.", protocol.c_str(), name.c_str(), path.c_str());
		}

		if (!backend)
			return false;

		backend->setEnabled(enabled);
		backend->setRole(role);
		backend->healthMonitor()->setMode(hcMode);
		backend->healthMonitor()->setInterval(hcInterval);
	}

	storagePath_ = path;

	setMutable(true);

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

	for (auto& br: backends_) {
		for (auto b: br) {
			out << "[backend=" << b->name() << "]\n"
				<< "role=" << b->role_str() << "\n"
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
