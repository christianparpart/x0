/* <x0/connection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/connection.hpp>
#include <x0/listener.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/server.hpp>
#include <x0/types.hpp>
#include <x0/sysconfig.h>

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#	include <gnutls/extra.h>
#endif

#include <functional>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define SSL_DEBUG(msg...) printf("SSL: " msg)

#define GNUTLS_CHECK(expr) { \
	int rv = (expr); \
		if (rv < 0) { \
		fprintf(stderr, "GNUTLS ERROR(%d): %s\n", rv, gnutls_strerror(rv)); \
		fprintf(stderr, "    AT LINE: " #expr "\n"); \
		fflush(stderr); \
	} \
}

namespace x0 {

connection::connection(x0::listener& lst) :
	secure(false),
	listener_(lst),
	server_(lst.server()),
//	socket_(), // initialized in constructor body
	remote_ip_(),
	remote_port_(0),
	buffer_(8192),
	request_(new request(*this)),
	request_parser_(),
	watcher_(server_.loop()),
	timer_(server_.loop())
{
	//DEBUG("connection(%p)", this);

	socklen_t slen = sizeof(saddr_);
	memset(&saddr_, 0, slen);

	socket_ = ::accept(listener_.handle(), reinterpret_cast<sockaddr *>(&saddr_), &slen);

	if (socket_ < 0)
		throw std::runtime_error(strerror(errno));

	if (fcntl(socket_, F_SETFL, O_NONBLOCK) < 0)
		printf("could not set server socket into non-blocking mode: %s\n", strerror(errno));

#if defined(TCP_CORK)
	{
		int flag = 1;
		setsockopt(socket_, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
	}
#endif

#if 0 // defined(TCP_NODELAY)
	{
		int flag = 1;
		setsockopt(socket_, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag));
	}
#endif

	watcher_.set<connection, &connection::io_callback>(this);
	timer_.set<connection, &connection::timeout_callback>(this);

	server_.connection_open(this);
}

connection::~connection()
{
	//DEBUG("~connection(%p)", this);

	try
	{
		server_.connection_close(this); // we cannot pass a shared pointer here as use_count is already zero and it would just lead into an exception though
	}
	catch (...)
	{
		DEBUG("~connection(%p): unexpected exception", this);
	}

#if defined(WITH_SSL)
	if (ssl_enabled())
		gnutls_deinit(ssl_session_);
#endif

	::close(socket_);

	if (request_) {
		delete request_;
	}
}

void connection::io_callback(ev::io& w, int revents)
{
	watcher_.stop();
	timer_.stop();
	//ev_unloop(loop(), EVUNLOOP_ONE);

	if (revents & ev::READ)
		handle_read();

	if (revents & ev::WRITE)
		handle_write();
}

void connection::timeout_callback(ev::timer& watcher, int revents)
{
	handle_timeout();
}

#if defined(WITH_SSL)
void connection::ssl_initialize()
{
	gnutls_init(&ssl_session_, GNUTLS_SERVER);
	gnutls_priority_set(ssl_session_, listener_.priority_cache_);
	gnutls_credentials_set(ssl_session_, GNUTLS_CRD_CERTIFICATE, listener_.x509_cred_);

	gnutls_certificate_server_set_request(ssl_session_, GNUTLS_CERT_REQUEST);

	gnutls_session_enable_compatibility_mode(ssl_session_);

	gnutls_transport_set_ptr(ssl_session_, (gnutls_transport_ptr_t)handle());
}
#endif

#if defined(WITH_SSL)
bool connection::ssl_enabled() const
{
	return listener_.secure();
}
#endif

void connection::start()
{
	//DEBUG("connection(%p).start()", this);

#if defined(WITH_SSL)
	if (ssl_enabled())
	{
		state_ = handshaking;
		ssl_initialize();

		ssl_handshake();
		return;
	}
	else
		state_ = requesting;
#endif

#if defined(TCP_DEFER_ACCEPT)
	// it is ensured, that we have data pending, so directly start reading
	handle_read();
#else
	// client connected, but we do not yet know if we have data pending
	async_read_some();
#endif
}

#if defined(WITH_SSL)
bool connection::ssl_handshake()
{
	int rv = gnutls_handshake(ssl_session_);
	if (rv == GNUTLS_E_SUCCESS)
	{
		// handshake either completed or failed
		printf("---- handshake completed\n");
		state_ = requesting;
		async_read_some();
		return true;
	}

	if (rv != GNUTLS_E_AGAIN && rv != GNUTLS_E_INTERRUPTED)
	{
#if !defined(NDEBUG)
		fprintf(stderr, "SSL handshake failed (%d): %s\n", rv, gnutls_strerror(rv));
		fflush(stderr);
#endif
		delete this;
		return false;
	}

	fprintf(stderr, "SSL handshake incomplete (%d): %s\n", rv, gnutls_strerror(rv));
	printf("direction: %d\n", gnutls_record_get_direction(ssl_session_));

	switch (gnutls_record_get_direction(ssl_session_))
	{
		case 0: // read
			async_read_some();
			break;
		case 1: // write
			async_write_some();
			break;
		default:
			break;
	}
	return false;
}
#endif

/** processes the next request on the connection. */
void connection::resume()
{
	//DEBUG("connection(%p).resume()", this);

	buffer_.clear(); //! \todo on pipelined HTTP requests, this shouldn't be done.
	request_parser_.reset();
	request_ = new request(*this);

	async_read_some();
}

void connection::async_read_some()
{
	if (server_.max_read_idle() != -1)
		timer_.start(server_.max_read_idle(), 0.0);

	watcher_.start(socket_, ev::READ);
}

void connection::async_write_some()
{
	if (server_.max_write_idle() != -1)
		timer_.start(server_.max_write_idle(), 0.0);

	watcher_.start(socket_, ev::WRITE);
}

void connection::handle_write()
{
	//DEBUG("connection(%p).handle_write()", this);
#if defined(WITH_SSL)
	if (state_ == handshaking)
		return (void) ssl_handshake();

//	if (ssl_enabled())
//		rv = gnutls_write(ssl_session_, buffer_.capacity() - buffer_.size());
//	else if (write_some)
//		write_some(this);
#else
#endif

#if 0
	buffer write_buffer_;
	int write_offset_ = 0;

	int rv = ::write(socket_, write_buffer_.begin() + write_offset_, write_buffer_.size() - write_offset_);

	if (rv < 0)
		; // error
	else
	{
		write_offset_ += rv;
		async_write_some();
	}
#else
	if (write_some)
		write_some(this);
#endif
}

/**
 * This method gets invoked when there is data in our connection ready to read.
 *
 * We assume, that we are in request-parsing state.
 */
void connection::handle_read()
{
	//DEBUG("connection(%p).handle_read()", this);
#if defined(WITH_SSL)
	if (state_ == handshaking)
		return (void) ssl_handshake();
#endif

	std::size_t lower_bound = buffer_.size();
	int rv;

#if defined(WITH_SSL)
	if (ssl_enabled())
		rv = gnutls_read(ssl_session_, buffer_.end(), buffer_.capacity() - buffer_.size());
	else
		rv = ::read(socket_, buffer_.end(), buffer_.capacity() - buffer_.size());
#else
	rv = ::read(socket_, buffer_.end(), buffer_.capacity() - buffer_.size());
#endif
//	printf("connection::handle_read(): read %d bytes (%s)\n", rv, strerror(errno));

	if (rv < 0)
		; // error
	else if (rv == 0)
		; // EOF
	else
	{
		buffer_.resize(lower_bound + rv);

		// parse request (partial)
		boost::tribool result = request_parser_.parse(*request_, buffer_.ref(lower_bound, rv));

		if (result) // request fully parsed
		{
			if (response *response_ = new response(this, request_))
			{
				request_ = 0;

				try
				{
					server_.handle_request(response_->request(), response_);
				}
				catch (const host_not_found& e)
				{
//					fprintf(stderr, "exception caught: %s\n", e.what());
//					fflush(stderr);
					response_->status = 404;
					response_->finish();
				}
				catch (response::code_type reply)
				{
//					fprintf(stderr, "response::code exception caught (%d %s)\n", reply, response::status_cstr(reply));
//					fflush(stderr);
					response_->status = reply;
					response_->finish();
				}
			}
		}
		else if (!result) // received an invalid request
		{
			// -> send stock response: BAD_REQUEST
			(new response(this, request_, response::bad_request))->finish();
			request_ = 0;
		}
		else // result indeterminate: request still incomplete
		{
			// -> continue reading for request
			async_read_some();
		}
	}
}

void connection::handle_timeout()
{
	//DEBUG("connection(%p): timed out", this);

	watcher_.stop();
	ev_unloop(loop(), EVUNLOOP_ONE);

	delete this;
}

std::string connection::remote_ip() const
{
	if (remote_ip_.empty())
	{
		char buf[128];

		if (inet_ntop(AF_INET6, &saddr_.sin6_addr, buf, sizeof(buf)))
			remote_ip_ = buf;
	}

	return remote_ip_;
}

int connection::remote_port() const
{
	if (!remote_port_)
	{
		remote_port_ = ntohs(saddr_.sin6_port);
	}

	return remote_port_;
}

std::string connection::local_ip() const
{
	return std::string(); //! \todo implementation
}

int connection::local_port() const
{
	return 0; //! \todo implementation
}

} // namespace x0
