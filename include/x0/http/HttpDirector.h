#pragma once

#include <x0/Api.h>
#include <x0/Logging.h>
#include <x0/http/HttpRequest.h>
#include <ev++.h>

namespace x0 {

class HttpBackend;

/*!
 * \brief Load balancing HTTP request proxy.
 *
 * A \c HttpDirector implements load balancing over multiple \c HttpBackend
 * instances of different transport type.
 * It supports weights and multiple states, such as (online/offline)
 * and (active/standby).
 *
 * \todo thread safety for actual horizontal scalability.
 * \todo periodic health checks
 * \todo support requeuing requests when designated backend did not respond in time.
 */
class X0_API HttpDirector :
#ifndef NDEBUG
	public Logging
#endif
{
private:
	HttpWorker* worker_;

	//! directors name, as used for debugging and displaying.
	std::string name_;

	//! set of backends managed by this director.
	std::vector<HttpBackend*> backends_;

	//! list of queued requests.
	std::deque<HttpRequest*> queue_;

	//! total number of requests being processed by this director
	size_t total_;

	//! last backend-index a request has been successfully served with
	size_t lastBackend_;

	//! whether or not to cloak origin "Server" response-header
	bool cloakOrigin_;

	//! number of attempts to pass request to a backend before giving up
	size_t maxRetryCount_;

public:
	HttpDirector(HttpWorker* worker, const std::string& name);
	~HttpDirector();

	const std::string& name() const { return name_; }

	size_t capacity() const;
	size_t load() const;
	size_t total() const { return total_; }
	size_t queued() const { return queue_.size(); }

	const std::vector<HttpBackend*>& backends() const { return backends_; }

	bool cloakOrigin() const { return cloakOrigin_; }
	void setCloakOrigin(bool value) { cloakOrigin_ = value; }

	size_t maxRetryCount() const { return maxRetryCount_; }
	void setMaxRetryCount(size_t value) { maxRetryCount_ = value; }

	HttpBackend* createBackend(const std::string& name, const std::string& url);

	HttpBackend* createBackend(const std::string& name, const std::string& protocol, const std::string& hostname,
		int port, const std::string& path, const std::string& query);

	template<typename T, typename... Args>
	HttpBackend* createBackend(const std::string& name, size_t capacity, const Args&... args) {
		T* backend = new T(this, name, capacity, args...);
		backends_.push_back(backend);
		return backend;
	}

	void schedule(HttpRequest* r);
	bool reschedule(HttpRequest* r, HttpBackend* backend);

private:
	HttpBackend* selectBackend(HttpRequest* r);
	HttpBackend* nextBackend(HttpBackend* backend, HttpRequest* r);
	void enqueue(HttpRequest* r);
	void hit();
	void put(HttpBackend* backend);

	void onStop();

	friend class HttpBackend;
};

} // namespace x0

