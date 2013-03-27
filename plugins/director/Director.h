/* <plugin/director/Director.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "BackendCluster.h"
#include "Backend.h"
#include "BackendManager.h"
#include "Scheduler.h"

#include <x0/Counter.h>
#include <x0/Logging.h>
#include <x0/DateTime.h>
#include <x0/http/HttpRequest.h>
#include <x0/CustomDataMgr.h>
#include <x0/JsonWriter.h>
#include <ev++.h>

#include <list>
#include <functional>

using namespace x0;

class Scheduler;
class RequestNotes;

enum class BackendRole {
	Active,
	Standby,
	Backup,
	Terminate,
};

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

	bool stickyOfflineMode_;    //!< whether a backend should be marked disabled if it becomes online again

	std::vector<BackendCluster> backends_; //!< set of backends managed by this director.

	size_t queueLimit_;         //!< how many requests to queue in total.
	TimeSpan queueTimeout_;		//!< how long a request may be queued.
	TimeSpan retryAfter_;       //!< time a client should wait before retrying a failed request.
	size_t maxRetryCount_;      //!< number of attempts to pass request to a backend before giving up
	std::string storagePath_;   //!< path to the local directory this director is serialized from/to.
	Scheduler* scheduler_;      //!< request scheduler

	x0::Counter load_;
	x0::Counter queued_;
	std::atomic<unsigned long long> dropped_;

	std::deque<x0::HttpRequest*> queue_; //! list of queued requests.
	std::mutex queueLock_;
	ev::timer queueTimer_;

	std::list<std::function<void()>>::iterator stopHandle_;

public:
	Director(HttpWorker* worker, const std::string& name);
	~Director();

	const x0::Counter& load() const { return load_; }
	const x0::Counter& queued() const { return queued_; }

	void schedule(x0::HttpRequest* r);
	virtual void reject(x0::HttpRequest* r);
	virtual void release(Backend* backend);

	bool isMutable() const { return mutable_; }
	void setMutable(bool value) { mutable_ = value; }

	size_t capacity() const;

	const std::string& healthCheckHostHeader() const { return healthCheckHostHeader_; }
	void setHealthCheckHostHeader(const std::string& value) { healthCheckHostHeader_ = value; }

	const std::string& healthCheckRequestPath() const { return healthCheckRequestPath_; }
	void setHealthCheckRequestPath(const std::string& value) { healthCheckRequestPath_ = value; }

	const std::string& healthCheckFcgiScriptFilename() const { return healthCheckFcgiScriptFilename_; }
	void setHealthCheckFcgiScriptFilename(const std::string& value) { healthCheckFcgiScriptFilename_ = value; }

	bool stickyOfflineMode() const { return stickyOfflineMode_; }
	void setStickyOfflineMode(bool value) { stickyOfflineMode_ = value; }

	size_t queueLimit() const { return queueLimit_; }
	void setQueueLimit(size_t value) { queueLimit_ = value; }

	TimeSpan queueTimeout() const { return queueTimeout_; }
	void setQueueTimeout(TimeSpan value) { queueTimeout_ = value; }

	TimeSpan retryAfter() const { return retryAfter_; }
	void setRetryAfter(TimeSpan value) { retryAfter_ = value; }

	size_t maxRetryCount() const { return maxRetryCount_; }
	void setMaxRetryCount(size_t value) { maxRetryCount_ = value; }

	RequestNotes* setupRequestNotes(x0::HttpRequest* r, Backend* backend = nullptr);
	RequestNotes* requestNotes(x0::HttpRequest* r);

	Backend* createBackend(const std::string& name, const Url& url);
	Backend* createBackend(const std::string& name, const std::string& protocol, const x0::SocketSpec& spec, size_t capacity, BackendRole role);
	void terminateBackend(Backend* backend);

	Backend* findBackend(const std::string& name);

	template<typename T>
	inline void eachBackend(T callback)
	{
		for (auto& br: backends_) {
			for (auto b: br) {
				callback(b);
			}
		}
	}

	const std::vector<Backend*>& backendsWith(BackendRole role) const;

	void writeJSON(x0::JsonWriter& output) const;

	bool load(const std::string& path);
	bool save();

	BackendRole backendRole(const Backend* backend) const;
	void setBackendRole(Backend* backend, BackendRole role);

private:
	void onBackendStateChanged(Backend* backend, HealthMonitor* healthMonitor);
	void link(Backend* backend, BackendRole role);
	Backend* unlink(Backend* backend);

	void onStop();

	SchedulerStatus tryProcess(x0::HttpRequest* r, BackendRole role);
	SchedulerStatus tryProcess(x0::HttpRequest* r, Backend* backend);
	bool tryEnqueue(x0::HttpRequest* r);
	void dequeueTo(Backend* backend);
	void updateQueueTimer();
	x0::HttpRequest* dequeue();

	void serviceUnavailable(x0::HttpRequest* r);
};

namespace x0 {
	JsonWriter& operator<<(JsonWriter& json, const Director& director);
}

// {{{ inlines
inline const std::vector<Backend*>& Director::backendsWith(BackendRole role) const
{
	return backends_[static_cast<size_t>(role)].cluster();
}
// }}}
