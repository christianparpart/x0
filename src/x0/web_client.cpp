/* <x0/web_client.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/web_client.hpp>
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
	message_(),
	request_buffer_(),
	request_offset_(0),
	flush_offset_(0),
	response_buffer_(),
	response_parser_(response_parser::ALL),
	connect_timeout(0),
	write_timeout(0),
	read_timeout(0),
	keepalive_timeout(0),
	on_response(),
	on_header(),
	on_content(),
	on_complete()
{
	io_.set<web_client, &web_client::io>(this);
	timer_.set<web_client, &web_client::timeout>(this);

	using namespace std::placeholders;
	response_parser_.status = std::bind(&web_client::_on_status, this, _1, _2, _3);
	response_parser_.assign_header = std::bind(&web_client::_on_header, this, _1, _2);
	response_parser_.process_content = std::bind(&web_client::_on_content, this, _1);
}

web_client::~web_client()
{
	close();
}

void web_client::open(const std::string& hostname, int port)
{
#if 0 // IPv6
	sockaddr_in6 sin;
	memset(&sin, 0, sizeof(sin));

	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons(port);

	if (inet_pton(sin.sin6_family, hostname.c_str(), sin.sin6_addr.s6_addr) < 0)
	{
		TRACE("async_connect: resolve error: %s", strerror(errno));
		return;
	}

	socklen_t slen = sizeof(sockaddr_in6);

	fd_ = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
#else // IPv4-only (good for debugging, as ngrep catches the I/O)
	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if (inet_pton(sin.sin_family, hostname.c_str(), &sin.sin_addr) < 0)
	{
		TRACE("async_connect: resolve error: %s", strerror(errno));
		return;
	}

	socklen_t slen = sizeof(sockaddr_in);

	fd_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif

	if (fd_ < 0)
		return;

	int rv = fcntl(fd_, F_GETFL, NULL) | O_NONBLOCK | O_CLOEXEC;
	fcntl(fd_, F_SETFL, &rv, sizeof(rv));

	rv = ::connect(fd_, (sockaddr *)&sin, slen);

	if (rv == -1)
	{
		if (errno != EINPROGRESS)
		{
			TRACE("async_connect error: %s", strerror(errno));

			close();
		}
		else
		{
			TRACE("async_connect: backgrounding");

			state_ = CONNECTING;
			start_write();
		}
	}
	else
	{
		state_ = CONNECTED;
		TRACE("async_connect: instant success");
	}
}

bool web_client::is_open() const
{
	return fd_ >= 0;
}

void web_client::close()
{
	if (state_ != DISCONNECTED)
	{
		if (on_complete)
			on_complete();
	}

	if (fd_ >= 0)
	{
		::close(fd_);
		fd_ = -1;
	}
	state_ = DISCONNECTED;
}

std::string web_client::message() const
{
	return message_;
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
			break;
		case CONNECTING:
			if (connect_timeout > 0)
				timer_.start(connect_timeout, 0.0);

			io_.set(fd_, ev::WRITE);
			io_.start();
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
			TRACE("async_connect: connected");
			if (flush_offset_)
				start_write(); // some request got already committed -> start write immediately
			else
				start_read(); // we're idle, watch for EOF
		}
		else
		{
			message_ = strerror(val);
			TRACE("async_connect: error(%d): %s", val, message_.c_str());
			close();
		}
	}
	else
	{
		message_ = strerror(errno);
		TRACE("on_connect_done: getsocketopt() error: %s", message_.c_str());
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
	if (equals(name, "Content-Length"))
		;//response_content_length = value.as<int>();

	if (on_header)
		on_header(name, value);
}

void web_client::_on_content(const buffer_ref& chunk)
{
	if (on_content)
		on_content(chunk);

	if (false && on_complete)
		on_complete();
}

} // namespace x0
