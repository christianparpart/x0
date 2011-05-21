/* <HttpListener.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_listener_h
#define x0_listener_h (1)

#include <x0/http/HttpServer.h>
#include <x0/ServerSocket.h>
#include <x0/SocketDriver.h>
#include <x0/Severity.h>
#include <x0/Types.h>
#include <x0/Api.h>
#include <x0/sysconfig.h>

#include <ev++.h>

#include <functional>
#include <memory>
#include <string>

#include <netdb.h> // struct addrinfo*

namespace x0 {

//! \addtogroup http
//@{

class HttpServer;
class HttpConnection;

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
class X0_API HttpListener
#if !defined(NDEBUG)
	: public Logging
#endif
{
public:
	explicit HttpListener(HttpServer& srv);
	~HttpListener();

	int backlog() const;
	void setBacklog(int value);

	bool open(const std::string& unixPath);
	bool open(const std::string& ip, int port);
	int handle() const;

	ServerSocket& socket();
	const ServerSocket& socket() const;
	HttpServer& server() const;

	bool isSecure() const;

	bool active() const;
	void stop();

	int errorCount() const;

	int addressFamily() const;

private:
	template<typename... Args>
	inline void log(Severity sv, const char *msg, Args&&... args);

	inline void setsockopt(int socket, int layer, int option, int value);
	void handle_accept();

	void callback(Socket*, ServerSocket*);

	struct ::ev_loop *loop() const;

private:
	ServerSocket socket_;
	HttpServer& server_;
	unsigned errorCount_;

	friend class HttpConnection;
};

// {{{ inlines
inline bool HttpListener::active() const
{
	return socket_.isOpen();
}

inline int HttpListener::errorCount() const
{
	return errorCount_;
}

inline int HttpListener::handle() const
{
	return socket_.handle();
}

template<typename... Args>
inline void HttpListener::log(Severity sv, const char *msg, Args&&... args)
{
	if (sv <= Severity::error)
		++errorCount_;

	server_.log(sv, msg, args...);
}

inline struct ::ev_loop *HttpListener::loop() const
{
	return server_.loop();
}

inline ServerSocket& HttpListener::socket()
{
	return socket_;
}

inline const ServerSocket& HttpListener::socket() const
{
	return socket_;
}

inline HttpServer& HttpListener::server() const
{
	return server_;
}

inline bool HttpListener::isSecure() const
{
#if defined(WITH_SSL)
	return socket_.socketDriver()->isSecure();
#else
	return false;
#endif
}
//@}

// }}}

} // namespace x0

#endif
