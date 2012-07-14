#pragma once

#include <x0/Api.h>
#include <x0/Logging.h>
#include <x0/http/HttpRequest.h>

namespace x0 {

class HttpDirector;

/*!
 * \brief abstract base class for the actual proxying instances as used by \c HttpDirector.
 *
 * \see HttpProxy, FastCgiProxy
 */
class X0_API HttpBackend
#ifndef NDEBUG
	: public Logging
#endif
{
public:
	enum class State {
		Online,
		Offline,
	};

protected:
	//! director, this backend is registered to
	HttpDirector* director_;
	//! common name of this backend, for example: "appserver05"
	std::string name_;
	//! number of concurrent requests being processable at a time.
	size_t capacity_;
	//! number of milliseconds to wait until next check
	unsigned checkInterval_;
	State state_;
	bool enabled_;

	//! time in seconds this backend has not been available for processing requests
	time_t offlineTime_;

public:
	HttpBackend(HttpDirector* director, const std::string& name, size_t capacity = 1);
	virtual ~HttpBackend();

	const std::string& name() const { return name_; }
	HttpDirector* director() const { return director_; } //!< pointer to the owning director.
	size_t capacity() const;		//!< number of requests this backend can handle in parallel.

	virtual size_t load() const;	//!< number of currently being processed requests.

	unsigned checkInterval() const { return checkInterval_; }
	void setCheckInterval(unsigned ms);

	State state() const { return state_; }

	void enable() { enabled_ = true; }
	bool enabled() const { return enabled_; }
	void disable() { enabled_ = false; }

	virtual bool process(HttpRequest* r) = 0;

	//! create a readable string containing the backend's state, i.e.
	//! "HttpBackend<appserver05: role=active, state=online, capacity=8, size=7>"
	virtual std::string str() const;

private:
	void onCheckState(ev::timer&, int);
};

/*! dummy proxy, just returning 503 (service unavailable).
 */
class X0_API NullProxy : public HttpBackend {
public:
	NullProxy(HttpDirector* director, const std::string& name, size_t capacity);

	virtual bool process(HttpRequest* r);
};

/*!
 * \brief implements an HTTP (reverse) proxy.
 *
 * \see FastCgiProxy
 */
class X0_API HttpProxy : public HttpBackend {
private:
	class ProxyConnection;

	std::string hostname_;
	int port_;
	size_t active_; //!< number of active (busy) connections
	std::list<ProxyConnection*> connections_;

public:
	//HttpProxy(HttpDirector* director, const std::string& name, size_t capacity, const std::string& url);
	HttpProxy(HttpDirector* director, const std::string& name, size_t capacity, const std::string& hsotname, int port);
	~HttpProxy();

	virtual bool process(HttpRequest* r);

	virtual size_t load() const;

private:
	ProxyConnection* acquireConnection();
};

class X0_API FastCgiProxy : public HttpBackend {
public:
	FastCgiProxy(HttpDirector* director, const std::string& name, size_t capacity, const std::string& url);
	~FastCgiProxy();

	virtual bool process(HttpRequest* r);
};

} // namespace x0
