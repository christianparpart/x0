/* <plugin/director/Director.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include "Backend.h"

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
class Director
#ifndef NDEBUG
	: public Logging
#endif
{
private:
	HttpWorker* worker_;

	//! directors name, as used for debugging and displaying.
	std::string name_;

	bool mutable_; //!< whether or not one may create/update/delete backends at runtime

	std::string healthCheckHostHeader_;
	std::string healthCheckRequestPath_;
	std::string healthCheckFcgiScriptFilename_;

	bool stickyOfflineMode_;

	// set of backends managed by this director.
	std::vector<std::vector<Backend*>> backends_;

	size_t queueLimit_;
	TimeSpan queueTimeout_;

	TimeSpan retryAfter_;

	TimeSpan connectTimeout_;
	TimeSpan readTimeout_;
	TimeSpan writeTimeout_;

	//! number of attempts to pass request to a backend before giving up
	size_t maxRetryCount_;

	std::string storagePath_;

	Scheduler* scheduler_;

	std::list<std::function<void()>>::iterator stopHandle_;

public:
	Director(HttpWorker* worker, const std::string& name);
	~Director();

	HttpWorker& worker() { return *worker_; }

	const std::string& name() const { return name_; }

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

	TimeSpan connectTimeout() const { return connectTimeout_; }
	void setConnectTimeout(TimeSpan value) { connectTimeout_ = value; }

	TimeSpan readTimeout() const { return readTimeout_; }
	void setReadTimeout(TimeSpan value) { readTimeout_ = value; }

	TimeSpan writeTimeout() const { return writeTimeout_; }
	void setWriteTimeout(TimeSpan value) { writeTimeout_ = value; }

	size_t maxRetryCount() const { return maxRetryCount_; }
	void setMaxRetryCount(size_t value) { maxRetryCount_ = value; }

	Scheduler* scheduler() const { return scheduler_; }

	RequestNotes* setupRequestNotes(x0::HttpRequest* r, Backend* backend = nullptr);
	RequestNotes* requestNotes(x0::HttpRequest* r);

	Backend* createBackend(const std::string& name, const std::string& url);

	Backend* createBackend(const std::string& name, const std::string& protocol, const std::string& hostname,
		int port, const std::string& path, const std::string& query);

	template<typename T, typename... Args>
	Backend* createBackend(const std::string& name, const SocketSpec& ss, size_t capacity, const Args&... args) {
		return new T(this, name, ss, capacity, args...);
	}

	Backend* findBackend(const std::string& name);

	void writeJSON(x0::JsonWriter& output) const;

	bool load(const std::string& path);
	bool save();

	template<typename T>
	inline void eachBackend(T callback)
	{
		for (auto& br: backends_) {
			for (auto b: br) {
				callback(b);
			}
		}
	}

	template<typename T> inline void post(T function) { worker_->post(function); }

	const std::vector<Backend*>& backendsWith(Backend::Role role) const;

private:
	void link(Backend* backend);
	void unlink(Backend* backend);

	void onStop();

	friend class Backend;
};

namespace x0 {
	JsonWriter& operator<<(JsonWriter& json, const Director& director);
}

// {{{ inlines
inline const std::vector<Backend*>& Director::backendsWith(Backend::Role role) const
{
	return backends_[static_cast<size_t>(role)];
}
// }}}
