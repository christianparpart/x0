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

#if 0
#	undef DEBUG
#	define DEBUG(x...) /*!*/
#endif

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
	response_(0),
	state_(invalid),
	watcher_(server_.loop())
#if defined(WITH_CONNECTION_TIMEOUTS)
	, timer_(server_.loop())
#endif
#if !defined(NDEBUG)
	, ctime_(ev_now(server_.loop()))
#endif
{
	DEBUG("connection(%p)", this);

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

#if defined(WITH_CONNECTION_TIMEOUTS)
	timer_.set<connection, &connection::timeout_callback>(this);
#endif

	server_.connection_open(this);
}

connection::~connection()
{
	delete request_;
	delete response_;
	request_ = 0;
	response_ = 0;

	DEBUG("~connection(%p)", this);

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
}

void connection::io_callback(ev::io& w, int revents)
{
	DEBUG("connection(%p).io_callback(revents=0x%04X)", this, revents);

#if defined(WITH_CONNECTION_TIMEOUTS)
	timer_.stop();
#endif

	if (revents & ev::READ)
		handle_read();

	if (revents & ev::WRITE)
		handle_write();
}

#if defined(WITH_CONNECTION_TIMEOUTS)
void connection::timeout_callback(ev::timer& watcher, int revents)
{
	handle_timeout();
}

void connection::handle_timeout()
{
	DEBUG("connection(%p): timed out", this);

	watcher_.stop();
//	ev_unloop(loop(), EVUNLOOP_ONE);

	delete this;
}
#endif

#if defined(WITH_SSL)
void connection::ssl_initialize()
{
	gnutls_init(&ssl_session_, GNUTLS_SERVER);
	gnutls_priority_set(ssl_session_, listener_.priority_cache_);
	gnutls_credentials_set(ssl_session_, GNUTLS_CRD_CERTIFICATE, listener_.x509_cred_);

	gnutls_certificate_server_set_request(ssl_session_, GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits(ssl_session_, 1024);

	gnutls_session_enable_compatibility_mode(ssl_session_);

	gnutls_transport_set_ptr(ssl_session_, (gnutls_transport_ptr_t)handle());

	listener_.ssl_db().bind(ssl_session_);
}

bool connection::ssl_enabled() const
{
	return listener_.secure();
}
#endif

void connection::start()
{
	DEBUG("connection(%p).start()", this);

#if defined(WITH_SSL)
	if (ssl_enabled())
	{
		handshaking_ = true;
		ssl_initialize();
		ssl_handshake();
		return;
	}
	else
		handshaking_ = false;
#endif

#if defined(TCP_DEFER_ACCEPT)
	// it is ensured, that we have data pending, so directly start reading
	handle_read();
#else
	// client connected, but we do not yet know if we have data pending
	start_read();
#endif
}

#if defined(WITH_SSL)
bool connection::ssl_handshake()
{
	int rv = gnutls_handshake(ssl_session_);
	if (rv == GNUTLS_E_SUCCESS)
	{
		// handshake either completed or failed
		handshaking_ = false;
		DEBUG("SSL handshake time: %.4f", ev_now(server_.loop()) - ctime_);
		start_read();
		return true;
	}

	if (rv != GNUTLS_E_AGAIN && rv != GNUTLS_E_INTERRUPTED)
	{
		DEBUG("SSL handshake failed (%d): %s", rv, gnutls_strerror(rv));

		delete this;
		return false;
	}

	DEBUG("SSL handshake incomplete: (%d)", gnutls_record_get_direction(ssl_session_));
	switch (gnutls_record_get_direction(ssl_session_))
	{
		case 0: // read
			start_read();
			break;
		case 1: // write
			start_write();
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
	DEBUG("connection(%p).resume()", this);

	delete request_;
	request_ = 0;

	delete response_;
	response_ = 0;

	std::size_t offset = request_parser_.next_offset();
	request_parser_.reset();
	request_ = new request(*this);

	if (offset < buffer_.size()) // HTTP pipelining
		parse_request(offset, buffer_.size() - offset);
	else
	{
		buffer_.clear();
		start_read();
	}
}

void connection::start_read()
{
	if (state_ != reading)
	{
		state_ = reading;
		watcher_.start(socket_, ev::READ);
	}

#if defined(WITH_CONNECTION_TIMEOUTS)
	if (server_.max_read_idle() > 0)
		timer_.start(server_.max_read_idle(), 0.0);
#endif
}

void connection::start_write()
{
	if (state_ != writing)
	{
		state_ = writing;
		watcher_.start(socket_, ev::WRITE);
	}

#if defined(WITH_CONNECTION_TIMEOUTS)
	if (server_.max_write_idle() > 0)
		timer_.start(server_.max_write_idle(), 0.0);
#endif
}

void connection::handle_write()
{
	DEBUG("connection(%p).handle_write()", this);

#if defined(WITH_SSL)
	if (handshaking_)
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
		start_write();
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
	DEBUG("connection(%p).handle_read()", this);

#if defined(WITH_SSL)
	if (handshaking_)
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

	if (rv < 0) // error
	{
		DEBUG("connection::handle_read(): %s", strerror(errno));
		delete this;
	}
	else if (rv == 0) // EOF
	{
		DEBUG("connection::handle_read(): (EOF) %s", strerror(errno));
		delete this;
	}
	else
	{
		;//DEBUG("connection::handle_read(): read %d bytes", rv);

		buffer_.resize(lower_bound + rv);
		parse_request(lower_bound, rv);
	}
}

/** parses (partial) request from buffer's given \p offset of \p count bytes.
 */
void connection::parse_request(std::size_t offset, std::size_t count)
{
	// parse request (partial)
	boost::tribool result = request_parser_.parse(*request_, buffer_.ref(offset, count));

	if (result) // request fully parsed
	{
		if (response *response_ = new response(this, request_))
		{
			try
			{
				server_.handle_request(response_->request(), response_);
			}
			catch (const host_not_found& e)
			{
//				fprintf(stderr, "exception caught: %s\n", e.what());
//				fflush(stderr);
				response_->status = 404;
				response_->finish();
			}
			catch (response::code_type reply)
			{
//				fprintf(stderr, "response::code exception caught (%d %s)\n", reply, response::status_cstr(reply));
//				fflush(stderr);
				response_->status = reply;
				response_->finish();
			}
		}
	}
	else if (!result) // received an invalid request
	{
		// -> send stock response: BAD_REQUEST
		(new response(this, request_, response::bad_request))->finish();
	}
	else // result indeterminate: request still incomplete
	{
		// -> continue reading for request
		start_read();
	}
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
	return listener_.address();
}

int connection::local_port() const
{
	return listener_.port();
}

} // namespace x0
