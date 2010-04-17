/* <x0/web_client.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/web_client.hpp>
#include <x0/gai_error.hpp>
#include <x0/buffer.hpp>
#include <ev++.h>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("web_client: " msg)
#endif

/**
 * \todo HTTP keep-alive (auto-enabled on keepalive_timeout > 0)
 * \todo HTTP pipelining
 * \todo SSL/TLS, if WITH_SSL is defined (no verification required)
 * \todo proper timeout handling, if WITH_CONNECTION_TIMEOUTS is defined
 * \todo chunk-encoded body (if content-length unknown at commit()-time)
 * \todo proper HTTP POST contents
 *       - the body is directly appended to the request buffer if (flush == false)
 */

namespace x0 {

web_client::web_client(struct ev_loop *loop) :
	loop_(loop),
	fd_(-1),
	state_(DISCONNECTED),
	io_(loop_),
	timer_(loop_),
	last_error_(),
	request_buffer_(),
	request_offset_(0),
	flush_offset_(0),
	response_buffer_(),
	response_parser_(response_parser::ALL),
	connect_timeout(0),
	write_timeout(0),
	read_timeout(0),
	keepalive_timeout(0),
	on_connect(),
	on_response(),
	on_header(),
	on_content(),
	on_complete()
{
	io_.set<web_client, &web_client::io>(this);
	timer_.set<web_client, &web_client::timeout>(this);

	using namespace std::placeholders;
	response_parser_.on_status = std::bind(&web_client::_on_status, this, _1, _2, _3);
	response_parser_.on_header = std::bind(&web_client::_on_header, this, _1, _2);
	response_parser_.on_content = std::bind(&web_client::_on_content, this, _1);
	response_parser_.on_complete = std::bind(&web_client::_on_complete, this);
}

web_client::~web_client()
{
	close();
}

bool web_client::open(const std::string& hostname, int port)
{
	TRACE("connect(hostname=%s, port=%d)", hostname.c_str(), port);

	struct addrinfo hints;
	struct addrinfo *res;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char sport[16];
	snprintf(sport, sizeof(sport), "%d", port);

	int rv = getaddrinfo(hostname.c_str(), sport, &hints, &res);
	if (rv)
	{
		last_error_ = make_error_code(static_cast<enum gai_error>(rv));
		TRACE("connect: resolve error: %s", last_error_.message().c_str());
		return false;
	}

	for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next)
	{
		fd_ = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd_ < 0)
		{
			last_error_ = std::make_error_code(static_cast<std::errc>(errno));
			continue;
		}

		int flags = fcntl(fd_, F_GETFL, NULL) | O_NONBLOCK | O_CLOEXEC;
		fcntl(fd_, F_SETFL, flags);

		rv = ::connect(fd_, rp->ai_addr, rp->ai_addrlen);
		if (rv < 0)
		{
			if (errno == EINPROGRESS)
			{
				TRACE("connect: backgrounding");
				start_write();
				break;
			}
			else
			{
				last_error_ = std::make_error_code(static_cast<std::errc>(errno));
				TRACE("connect error: %s (category: %s)", last_error_.message().c_str(), last_error_.category().name());
				close();
			}
		}
		else
		{
			state_ = CONNECTED;
			TRACE("connect: instant success");

			if (on_connect)
				on_connect();

			break;
		}
	}

	freeaddrinfo(res);

	return fd_ >= 0;
}

bool web_client::is_open() const
{
	return fd_ >= 0;
}

void web_client::close()
{
	if (state_ == DISCONNECTED)
		return;

	state_ = DISCONNECTED;

	::close(fd_);
	fd_ = -1;

	io_.stop();
	timer_.stop();
}

std::error_code web_client::last_error() const
{
	return last_error_;
}

void web_client::commit(bool flush)
{
	if (keepalive_timeout > 0)
		pass_header("Connection", "keep-alive");
	else
		pass_header("Connection", "close");

	request_buffer_.push_back("\015\012"); // final linefeed

	if (flush)
	{
		if (state_ == CONNECTED)
			start_write();
		else
			flush_offset_ = request_buffer_.size();
	}
}

void web_client::pause()
{
	if (timer_.is_active())
		timer_.stop();

	if (io_.is_active())
		io_.stop();
}

void web_client::resume()
{
	switch (state_)
	{
		case DISCONNECTED:
			break;
		case CONNECTING:
			if (connect_timeout > 0)
				timer_.start(connect_timeout, 0.0);

			io_.start();
			break;
		case CONNECTED:
			break;
		case WRITING:
			if (write_timeout > 0)
				timer_.start(write_timeout, 0.0);

			io_.start();
			break;
		case READING:
			if (read_timeout > 0)
				timer_.start(read_timeout, 0.0);

			io_.start();
			break;
	}
}

void web_client::start_read()
{
	TRACE("web_client: start_read(%d)", state_);
	switch (state_)
	{
		case DISCONNECTED:
			// invalid state to start reading from
			break;
		case CONNECTING:
			// we're invoked from within connect_done()
			state_ = CONNECTED;
			io_.set(fd_, ev::READ);

			if (on_connect)
				on_connect();

			break;
		case CONNECTED:
			// invalid state to start reading from
			break;
		case WRITING:
			if (read_timeout > 0)
				timer_.start(read_timeout, 0.0);

			state_ = READING;
			io_.set(fd_, ev::READ);
			break;
		case READING:
			// continue reading
			if (read_timeout > 0)
				timer_.start(read_timeout, 0.0);

			break;
	}
}

void web_client::start_write()
{
	TRACE("web_client: start_write(%d)", state_);
	switch (state_)
	{
		case DISCONNECTED:
			// initiated asynchronous connect: watch for completeness

			if (connect_timeout > 0)
				timer_.start(connect_timeout, 0.0);

			io_.set(fd_, ev::WRITE);
			io_.start();
			state_ = CONNECTING;
			break;
		case CONNECTING:
			// asynchronous-connect completed and request committed already: start writing

			if (write_timeout > 0)
				timer_.start(write_timeout, 0.0);

			state_ = WRITING;
			break;
		case CONNECTED:
			if (write_timeout > 0)
				timer_.start(write_timeout, 0.0);

			state_ = WRITING;
			io_.set(fd_, ev::WRITE);
			io_.start();
			break;
		case WRITING:
			// continue writing
			break;
		case READING:
			if (write_timeout > 0)
				timer_.start(write_timeout, 0.0);

			state_ = WRITING;
			io_.set(fd_, ev::WRITE);
			break;
	}
}

void web_client::io(ev::io&, int revents)
{
	TRACE("io(fd=%d, revents=0x%04x)", fd_, revents);

	if (timer_.is_active())
		timer_.stop();

	if (revents & ev::READ)
		read_some();

	if (revents & ev::WRITE)
	{
		if (state_ != CONNECTING)
			write_some();
		else
			connect_done();
	}
}

void web_client::timeout(ev::timer&, int revents)
{
	TRACE("timeout");
}

void web_client::connect_done()
{
	int val = 0;
	socklen_t vlen = sizeof(val);
	if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0)
	{
		if (val == 0)
		{
			TRACE("connect_done: connected (flush_offset=%ld)", flush_offset_);
			if (flush_offset_)
				start_write(); // some request got already committed -> start write immediately
			else
				start_read(); // we're idle, watch for EOF
		}
		else
		{
			last_error_ = std::make_error_code(static_cast<std::errc>(val));
			TRACE("connect_done: error(%d): %s", val, last_error_.message().c_str());
			close();
		}
	}
	else
	{
		last_error_ = std::make_error_code(static_cast<std::errc>(errno));
		TRACE("on_connect_done: getsocketopt() error: %s", last_error_.message().c_str());
		close();
	}
}

void web_client::write_some()
{
	TRACE("web_client(%p)::write_some()", this);

	ssize_t rv = ::write(fd_, request_buffer_.data() + request_offset_, request_buffer_.size() - request_offset_);

	if (rv > 0)
	{
		TRACE("write request: %ld (of %ld) bytes", rv, request_buffer_.size() - request_offset_);

		request_offset_ += rv;

		if (request_offset_ == request_buffer_.size()) // request fully transmitted, let's read response then.
			start_read();
	}
	else
	{
		TRACE("write request failed(%ld): %s", rv, strerror(errno));
		close();
	}
}

void web_client::read_some()
{
	TRACE("connection(%p)::read_some()", this);

	std::size_t lower_bound = response_buffer_.size();

	if (lower_bound == response_buffer_.capacity())
		response_buffer_.capacity(lower_bound + 4096);

	ssize_t rv = ::read(fd_, (char *)response_buffer_.end(), response_buffer_.capacity() - lower_bound);

	if (rv > 0)
	{
		TRACE("read response: %ld bytes", rv);
		response_buffer_.resize(lower_bound + rv);
		response_parser_.parse(response_buffer_.ref(lower_bound, rv));
	}
	else if (rv == 0)
	{
		TRACE("http server (%d) connection closed", fd_);
		close();
	}
	else
	{
		TRACE("read response failed(%ld): %s", rv, strerror(errno));

		if (errno == EAGAIN)
			start_read();
		else
			close();
	}
}

void web_client::_on_status(const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
{
	if (on_response)
		on_response(protocol, code, text);
}

void web_client::_on_header(const buffer_ref& name, const buffer_ref& value)
{
	if (on_header)
		on_header(name, value);
}

void web_client::_on_content(const buffer_ref& chunk)
{
	if (on_content)
		on_content(chunk);
}

void web_client::_on_complete()
{
	flush_offset_ = 0;

	if (on_complete)
		on_complete();

	if (is_open())
		read_some();
}

} // namespace x0
