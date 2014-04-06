#pragma once

/* <plugin/director/Director.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "BackendCluster.h"
#include "Backend.h"
#include "BackendManager.h"
#include "Scheduler.h"
#include "RequestNotes.h"
#include "HealthMonitor.h"

#include <x0/IniFile.h>
#include <x0/Counter.h>
#include <x0/Logging.h>
#include <x0/DateTime.h>
#include <x0/http/HttpRequest.h>
#include <x0/CustomDataMgr.h>
#include <x0/TokenShaper.h>
#include <x0/JsonWriter.h>
#include <ev++.h>

#include <list>
#include <functional>

class Scheduler;
class ObjectCache;

namespace x0 {
	class Url;
}

using namespace x0;

/**
 * Defines the role of a backend.
 */
enum class BackendRole {
	Active,    //!< backends that are potentially getting new requests scheduled.
	Backup,    //!< backends that are used when the active backends are all down.
	Terminate, //!< artificial role that contains all backends in termination-progress.
};

typedef x0::TokenShaper<RequestNotes> RequestShaper;

/*!
 * \brief Load balancing HTTP request proxy.
 *
 * A \c Director implements load balancing over multiple \c Backend
 * instances of different transport type.
 * It supports weights and multiple states, such as (online/offline)
 * and (active/standby).
 *
 * \todo thread safety for actual horizontal scalability.
 * \todo support requeuing requests when designated backend did not respond in time.
 */
class Director : public BackendManager
{
private:
	bool mutable_; //!< whether or not one may create/update/delete backends at runtime

	std::string healthCheckHostHeader_;
	std::string healthCheckRequestPath_;
	std::string healthCheckFcgiScriptFilename_;

	bool enabled_;				//!< whether this director actually load balances or raises a 503 when being disabled temporarily.
	bool stickyOfflineMode_;    //!< whether a backend should be marked disabled if it becomes online again
	bool allowXSendfile_;		//!< whether or not to evaluate the X-Sendfile response header.
    bool enqueueOnUnavailable_; //!< whether to enqueue or to 503 the request when the request could not be delivered (no backend is UP).

	std::vector<BackendCluster> backends_; //!< set of backends managed by this director.

	size_t queueLimit_;         //!< how many requests to queue in total.
	TimeSpan queueTimeout_;		//!< how long a request may be queued.
	TimeSpan retryAfter_;       //!< time a client should wait before retrying a failed request.
	size_t maxRetryCount_;      //!< number of attempts to pass request to a backend before giving up
	std::string storagePath_;   //!< path to the local directory this director is serialized from/to.

	RequestShaper shaper_;

	x0::Counter queued_;
	std::atomic<unsigned long long> dropped_;

#if defined(ENABLE_DIRECTOR_CACHE)
	ObjectCache* objectCache_;	//!< response object cache
#endif

	std::list<std::function<void()>>::iterator stopHandle_;

public:
	Director(HttpWorker* worker, const std::string& name);
	~Director();

#if defined(ENABLE_DIRECTOR_CACHE)
	ObjectCache& objectCache() const { return *objectCache_; }
#endif

	const x0::Counter& queued() const { return queued_; }

    const std::string& scheduler() const;
    bool setScheduler(const std::string& name);
    template<typename T> void setScheduler();

	void schedule(RequestNotes* rn, Backend* backend);
	void schedule(RequestNotes* rn, RequestShaper::Node* bucket);
	void reschedule(RequestNotes* rn);

	virtual void reject(RequestNotes* rn);
	virtual void release(RequestNotes* rn);

	bool isMutable() const { return mutable_; }
	void setMutable(bool value) { mutable_ = value; }

	bool isEnabled() const { return enabled_; }
	void setEnabled(bool value) { enabled_ = value; }
	void enable() { enabled_ = true; }
	void disable() { enabled_ = false; }

	size_t capacity() const;

	x0::TokenShaperError createBucket(const std::string& name, float rate, float ceil);
	RequestShaper::Node* findBucket(const std::string& name) const;
	RequestShaper::Node* rootBucket() const { return shaper()->rootNode(); }
	RequestShaper* shaper() { return &shaper_; }
	const RequestShaper* shaper() const { return &shaper_; }
	bool eachBucket(std::function<bool(RequestShaper::Node*)> body);

	const std::string& healthCheckHostHeader() const { return healthCheckHostHeader_; }
	void setHealthCheckHostHeader(const std::string& value) { healthCheckHostHeader_ = value; }

	const std::string& healthCheckRequestPath() const { return healthCheckRequestPath_; }
	void setHealthCheckRequestPath(const std::string& value) { healthCheckRequestPath_ = value; }

	const std::string& healthCheckFcgiScriptFilename() const { return healthCheckFcgiScriptFilename_; }
	void setHealthCheckFcgiScriptFilename(const std::string& value) { healthCheckFcgiScriptFilename_ = value; }

	bool stickyOfflineMode() const { return stickyOfflineMode_; }
	void setStickyOfflineMode(bool value) { stickyOfflineMode_ = value; }

	bool allowXSendfile() const { return allowXSendfile_; }
	void setAllowXSendfile(bool value) { allowXSendfile_ = value; }

    bool enqueueOnUnavailable() const { return enqueueOnUnavailable_; }
    void setEnqueueOnUnavailable(bool value) { enqueueOnUnavailable_ = value; }

	size_t queueLimit() const { return queueLimit_; }
	void setQueueLimit(size_t value) { queueLimit_ = value; }

	TimeSpan queueTimeout() const { return queueTimeout_; }
	void setQueueTimeout(TimeSpan value) { queueTimeout_ = value; }

	TimeSpan retryAfter() const { return retryAfter_; }
	void setRetryAfter(TimeSpan value) { retryAfter_ = value; }

	size_t maxRetryCount() const { return maxRetryCount_; }
	void setMaxRetryCount(size_t value) { maxRetryCount_ = value; }

	Backend* createBackend(const std::string& name, const Url& url);
	Backend* createBackend(const std::string& name, const std::string& protocol, const x0::SocketSpec& spec, size_t capacity, BackendRole role);
	void terminateBackend(Backend* backend);

	bool findBackend(const std::string& name, const std::function<void(Backend*)>& cb);
	Backend* findBackend(const std::string& name);

	void eachBackend(const std::function<void(Backend*)>& callback)
	{
		for (auto& br: backends_) {
			br.each([&](Backend* backend) {
				callback(backend);
			});
		}
	}

	const BackendCluster& backendsWith(BackendRole role) const;

	void writeJSON(x0::JsonWriter& output) const;

	using BackendManager::load;
	bool load(const std::string& path);
	bool save();

	BackendRole backendRole(const Backend* backend) const;
	void setBackendRole(Backend* backend, BackendRole role);

private:
	bool processCacheObject(RequestNotes* notes);
	bool loadBackend(const x0::IniFile& settings, const std::string& key);
	bool loadBucket(const x0::IniFile& settings, const std::string& key);
	void onTimeout(RequestNotes* rn);
	void onBackendEnabledChanged(const Backend* backend);
	void onBackendStateChanged(Backend* backend, HealthMonitor* healthMonitor, HealthState oldState);
	void link(Backend* backend, BackendRole role);
	Backend* unlink(Backend* backend);

	void onStop();

	bool verifyTryCount(RequestNotes* notes);
	SchedulerStatus tryProcess(RequestNotes* notes, BackendRole role);
	bool tryEnqueue(RequestNotes* notes);
	void dequeueTo(Backend* backend);
	void updateQueueTimer();
	RequestNotes* dequeue();

	void serviceUnavailable(RequestNotes* notes, x0::HttpStatus status = x0::HttpStatus::ServiceUnavailable);
};

namespace x0 {
	JsonWriter& operator<<(JsonWriter& json, const Director& director);
}

// {{{ inlines
template<typename T>
inline void Director::setScheduler()
{
    for (auto& br: backends_) {
        br.setScheduler<T>();
    }
}

inline const BackendCluster& Director::backendsWith(BackendRole role) const
{
	return backends_[static_cast<size_t>(role)];
}
// }}}
