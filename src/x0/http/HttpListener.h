/* <HttpListener.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_listener_h
#define x0_listener_h (1)

#include <x0/http/HttpServer.h>
#include <x0/SocketDriver.h>
#include <x0/Severity.h>
#include <x0/Types.h>
#include <x0/Api.h>
#include <x0/sysconfig.h>

#include <ev++.h>

#include <functional>
#include <memory>
#include <string>

namespace x0 {

//! \addtogroup core
//@{

class HttpServer;
class HttpConnection;
class SocketDriver;

/**
 * \brief TCP/IP listener for the HTTP protocol.
 *
 * This class implements the TCP/IP listener for the HTTP protocol.
 * It binds and listens on a given address:port pair and creates a
 * new connection object for each new incoming TCP/IP client to process
 * all requests incoming from this request.
 *
 * @see HttpServer
 * @see HttpConnection
 */
class HttpListener
{
public:
	explicit HttpListener(HttpServer& srv);
	~HttpListener();

	std::string address() const;
	void address(const std::string& value);

	int port() const;
	void port(int value);

	int backlog() const;
	void backlog(int value);

	HttpServer& server() const;

	SocketDriver *socketDriver() const;
	void setSocketDriver(SocketDriver *sd);

#if defined(WITH_SSL)
	bool isSecure() const;
#endif

	int handle() const;

	bool prepare();
	bool start();
	bool active() const;
	void stop();

	int error_count() const;

private:
	template<typename... Args>
	inline void log(Severity sv, const char *msg, Args&&... args);

	inline void setsockopt(int socket, int layer, int option, int value);
	void handle_accept();

	void callback(ev::io& watcher, int revents);

	struct ::ev_loop *loop() const;

private:
	ev::io watcher_;
	int fd_;
	HttpServer& server_;
	std::string address_;
	int port_;
	int backlog_;
	int errors_;
	SocketDriver *socketDriver_;

	friend class HttpConnection;
};

// {{{ inlines
inline bool HttpListener::active() const
{
	return fd_ != -1;
}

inline SocketDriver *HttpListener::socketDriver() const
{
	return socketDriver_;
}

inline int HttpListener::error_count() const
{
	return errors_;
}

template<typename... Args>
inline void HttpListener::log(Severity sv, const char *msg, Args&&... args)
{
	if (sv <= Severity::error)
		errors_++;

	server_.log(sv, msg, args...);
}

inline struct ::ev_loop *HttpListener::loop() const
{
	return server_.loop();
}

inline HttpServer& HttpListener::server() const
{
	return server_;
}

inline int HttpListener::handle() const
{
	return fd_;
}

#if defined(WITH_SSL)
inline bool HttpListener::isSecure() const
{
	return socketDriver_->isSecure();
}
#endif
//@}

// }}}

} // namespace x0

#endif
