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
#	include <x0/ssl_db_cache.hpp>
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

	std::string address() const;
	void address(const std::string& value);

	int port() const;
	void port(int value);

	x0::server& server() const;

#if defined(WITH_SSL)
	bool secure() const;
	void secure(bool value);

	ssl_db_cache& ssl_db();

	void crl_file(const std::string&);
	void trust_file(const std::string&);
	void key_file(const std::string&);
	void cert_file(const std::string&);
#endif

	int handle() const;

	void prepare();
	void start();
	bool active() const;
	void stop();

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
	ssl_db_cache ssl_db_;
	std::string crl_file_;
	std::string trust_file_;
	std::string key_file_;
	std::string cert_file_;

	gnutls_certificate_credentials_t x509_cred_;
	gnutls_dh_params_t dh_params_;
	gnutls_priority_t priority_cache_;
#endif

	request_handler_fn handler_;

	friend class connection;
};

// {{{ inlines
inline bool listener::active() const
{
	return fd_ != -1;
}

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

#if defined(WITH_SSL)
inline bool listener::secure() const
{
	return secure_;
}

inline ssl_db_cache& listener::ssl_db()
{
	return ssl_db_;
}
#endif
//@}

// }}}

} // namespace x0

#endif
