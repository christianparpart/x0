#pragma once

#include <x0/Api.h>
#include <x0/Logging.h>
#include <x0/http/HttpRequest.h>

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
 * \todo support requeuing requests when designated backend did not respond in time.
 */
class X0_API HttpDirector :
#ifndef NDEBUG
	public Logging
#endif
{
private:
	//! directors name, as used for debugging and displaying.
	std::string name_;

	//! set of backends managed by this director.
	std::vector<HttpBackend*> backends_;

	//! last backend-index a request has been successfully served with
	size_t lastBackend_;

	//! whether or not to cloak origin "Server" response-header
	bool cloakOrigin_;

	//! number of attempts to pass request to a backend before giving up
	size_t maxRetryCount_;

public:
	explicit HttpDirector(const std::string& name);
	~HttpDirector();

	const std::string& name() const { return name_; }

	size_t capacity() const;
	size_t load() const;

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

	void enqueue(HttpRequest* r);
	bool requeue(HttpRequest* r, HttpBackend* backend);

private:
	HttpBackend* selectBackend(HttpRequest* r);
	HttpBackend* nextBackend(HttpBackend* backend, HttpRequest* r);

	friend class HttpBackend;
};

} // namespace x0

