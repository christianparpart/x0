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
#include <x0/sysconfig.h>

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#endif

#include <ev++.h>

#include <functional>
#include <memory>
#include <string>

namespace x0 {

//! \addtogroup core
//@{

class server;
class connection;

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

#if defined(WITH_SSL)
	bool secure() const;
	void secure(bool value);
#endif

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

#if defined(WITH_SSL)
	bool secure_;

	gnutls_certificate_credentials_t x509_cred_;
	gnutls_dh_params_t dh_params_;
	gnutls_priority_t priority_cache_;
#endif

	request_handler_fn handler_;

	friend class connection;
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
