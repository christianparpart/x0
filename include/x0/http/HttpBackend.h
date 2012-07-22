#pragma once

#include <x0/Api.h>
#include <x0/Counter.h>
#include <x0/Logging.h>
#include <x0/TimeSpan.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHealthMonitor.h>

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
	enum class Role {
		Active,
		Standby,
	};

protected:
	HttpDirector* director_; //!< director, this backend is registered to

	std::string name_; //!< common name of this backend, for example: "appserver05"
	size_t capacity_; //!< number of concurrent requests being processable at a time.
	Counter load_; //!< number of active (busy) connections

	Role role_; //!< backend role (Active or Standby)
	bool enabled_; //!< whether or not this director is enabled (default) or disabled (for example for maintenance reasons)
	HttpHealthMonitor healthMonitor_; //!< health check timer

	friend class HttpDirector;

public:
	HttpBackend(HttpDirector* director, const std::string& name, size_t capacity = 1);
	virtual ~HttpBackend();

	const std::string& name() const { return name_; }		//!< descriptive name of backend.
	HttpDirector* director() const { return director_; }	//!< pointer to the owning director.
	size_t capacity() const;								//!< number of requests this backend can handle in parallel.
	const Counter& load() const { return load_; }					//!< number of currently being processed requests.

	// role
	Role role() const { return role_; }
	void setRole(Role value) { role_ = value; }

	// enable/disable state
	void enable() { enabled_ = true; }
	bool isEnabled() const { return enabled_; }
	void disable() { enabled_ = false; }

	// health state
	HttpHealthMonitor::State healthState() const { return healthMonitor_.state(); }
	HttpHealthMonitor& healthMonitor() { return healthMonitor_; }

	virtual bool process(HttpRequest* r) = 0;

	//! create a readable string containing the backend's state, i.e.
	//! "HttpBackend<appserver05: role=active, state=online, capacity=8, size=7>"
	virtual std::string str() const;

	virtual size_t writeJSON(Buffer& output) const;

	void release();

protected:
	void setState(HttpHealthMonitor::State value);

private:
	void startHealthChecks();
	void stopHealthChecks();
	void onHealthCheck(ev::timer&, int);
	void healthCheckHandler(ev::io&, int);
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

public:
	//HttpProxy(HttpDirector* director, const std::string& name, size_t capacity, const std::string& url);
	HttpProxy(HttpDirector* director, const std::string& name, size_t capacity, const std::string& hsotname, int port);
	~HttpProxy();

	virtual bool process(HttpRequest* r);
	virtual size_t writeJSON(Buffer& output) const;

private:
	ProxyConnection* acquireConnection();
};

class X0_API FastCgiProxy : public HttpBackend {
public:
	FastCgiProxy(HttpDirector* director, const std::string& name, size_t capacity, const std::string& url);
	~FastCgiProxy();

	virtual bool process(HttpRequest* r);
	virtual size_t writeJSON(Buffer& output) const;
};

} // namespace x0
