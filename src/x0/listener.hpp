/* <x0/listener.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_listener_hpp
#define x0_listener_hpp (1)

#include <x0/server.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>

#include <functional>
#include <memory>
#include <string>
#include <ev++.h>

namespace x0 {

//! \addtogroup core
//@{

class server;

/**
 * \brief TCP/IP listener for the HTTP protocol.
 *
 * This class implements the TCP/IP listener for the HTTP protocol.
 * It binds and listens on a given address:port pair and creates a
 * new connection object for each new incoming TCP/IP client to process
 * all requests incoming from this request.
 *
 * @see server
 * @see connection
 */
class listener
{
public:
	explicit listener(x0::server& srv);
	~listener();

	void configure(const std::string& address = "0::0", int port = 8080);
	void start();
	void stop();

	std::string address() const;
	int port() const;

	x0::server& server() const;

	int handle() const;

private:
	void handle_accept();

	void callback(ev::io& watcher, int revents);

	struct ::ev_loop *loop() const;

private:
	ev::io watcher_;
	int fd_;
	x0::server& server_;
	std::string address_;
	int port_;
	request_handler_fn handler_;
};

inline struct ::ev_loop *listener::loop() const
{
	return server_.loop();
}

inline x0::server& listener::server() const
{
	return server_;
}

inline int listener::handle() const
{
	return fd_;
}

//@}

} // namespace x0

#endif
