/* <x0/WebClient.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/WebClient.h>
#include <x0/Buffer.h>
#include <x0/gai_error.h>
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

#if 1
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("WebClient: " msg)
#endif

/**
 * \class WebClient
 * \brief HTTP/1.1 client API for connecting to HTTP/1.1 compliant servers.
 *
 * Features:
 * <ul>
 *   <li>Asynchronous I/O</li>
 *   <li>Keep-Alive connections</li>
 *   <li>Pipelined request/response handling</li>
 *   <li>Connection pause/resume functionality</li>
 * </ul>
 *
 * \todo proper HTTP POST contents (the body is directly appended to the request buffer if (flush == false))
 * \todo SSL/TLS, if WITH_SSL is defined (no verification required)
 *
 * \see message_parser
 */

namespace x0 {

WebClientBase::WebClientBase(struct ev_loop *loop) :
	HttpMessageProcessor(HttpMessageProcessor::RESPONSE),
	loop_(loop),
	fd_(-1),
	state_(DISCONNECTED),
	io_(loop_),
	timer_(loop_),
	last_error_(),
	request_buffer_(),
	request_offset_(0),
	request_count_(0),
	response_buffer_(),
	connect_timeout(0),
	write_timeout(0),
	read_timeout(0),
	keepalive_timeout(0)
{
	io_.set<WebClientBase, &WebClientBase::io>(this);
	timer_.set<WebClientBase, &WebClientBase::timeout>(this);
}

WebClientBase::~WebClientBase()
{
	close();
}

bool WebClientBase::open(const std::string& hostname, int port)
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
				startWrite();
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

			connect();

			break;
		}
	}

	freeaddrinfo(res);

	return fd_ >= 0;
}

bool WebClientBase::isOpen() const
{
	return fd_ >= 0;
}

void WebClientBase::close()
{
	state_ = DISCONNECTED;

	io_.stop();
	timer_.stop();

	if (fd_ >= 0)
	{
		::close(fd_);
		fd_ = -1;
	}
}

std::error_code WebClientBase::last_error() const
{
	return last_error_;
}

void WebClientBase::commit(bool flush)
{
	if (keepalive_timeout > 0)
		write_header("Connection", "keep-alive");
	else
		write_header("Connection", "close");

	request_buffer_.push_back("\015\012"); // final linefeed

	++request_count_;

	if (flush)
	{
		if (state_ == CONNECTED)
			startWrite();
	}
}

void WebClientBase::pause()
{
	if (timer_.is_active())
		timer_.stop();

	if (io_.is_active())
		io_.stop();
}

void WebClientBase::resume()
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

void WebClientBase::startRead()
{
	TRACE("WebClient: startRead(%d)", state_);
	switch (state_)
	{
		case DISCONNECTED:
			// invalid state to start reading from
			break;
		case CONNECTING:
			// we're invoked from within onConnectComplete()
			state_ = CONNECTED;
			io_.set(fd_, ev::READ);

			connect();

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

void WebClientBase::startWrite()
{
	TRACE("WebClientBase: startWrite(%d)", state_);
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

void WebClientBase::io(ev::io&, int revents)
{
	TRACE("io(fd=%d, revents=0x%04x)", fd_, revents);

	if (timer_.is_active())
		timer_.stop();

	if (revents & ev::READ)
		readSome();

	if (revents & ev::WRITE)
	{
		if (state_ != CONNECTING)
			writeSome();
		else
			onConnectComplete();
	}
}

void WebClientBase::timeout(ev::timer&, int revents)
{
	TRACE("timeout"); // TODO
}

void WebClientBase::onConnectComplete()
{
	int val = 0;
	socklen_t vlen = sizeof(val);
	if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0)
	{
		if (val == 0)
		{
			TRACE("onConnectComplete: connected (request_count=%ld)", request_count_);
			if (request_count_)
				startWrite(); // some request got already committed -> start write immediately
			else
				startRead(); // we're idle, watch for EOF
		}
		else
		{
			last_error_ = std::make_error_code(static_cast<std::errc>(val));
			TRACE("onConnectComplete: error(%d): %s", val, last_error_.message().c_str());
			close();
		}
	}
	else
	{
		last_error_ = std::make_error_code(static_cast<std::errc>(errno));
		TRACE("onConnectComplete: getsocketopt() error: %s", last_error_.message().c_str());
		close();
	}

	if (!isOpen())
		complete();
}

void WebClientBase::writeSome()
{
	TRACE("WebClient(%p)::writeSome()", this);

	ssize_t rv = ::write(fd_, request_buffer_.data() + request_offset_, request_buffer_.size() - request_offset_);

	if (rv > 0)
	{
		TRACE("write request: %ld (of %ld) bytes", rv, request_buffer_.size() - request_offset_);

		request_offset_ += rv;

		if (request_offset_ == request_buffer_.size()) // request fully transmitted, let's read response then.
			startRead();
	}
	else
	{
		TRACE("write request failed(%ld): %s", rv, strerror(errno));
		close();
	}
}

void WebClientBase::readSome()
{
	TRACE("connection(%p)::readSome()", this);

	std::size_t lower_bound = response_buffer_.size();

	if (lower_bound == response_buffer_.capacity())
		response_buffer_.capacity(lower_bound + 4096);

	ssize_t rv = ::read(fd_, (char *)response_buffer_.end(), response_buffer_.capacity() - lower_bound);

	if (rv > 0)
	{
		TRACE("read response: %ld bytes", rv);
		response_buffer_.resize(lower_bound + rv);

		std::size_t np = 0;
		std::error_code ec = process(response_buffer_.ref(lower_bound, rv), np);
		TRACE("readSome@process: %s", ec.message().c_str());
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
			startRead();
		else
			close();
	}
}

void WebClientBase::message_begin(int version_major, int version_minor, int code, BufferRef&& text)
{
	response(version_major, version_minor, code, std::move(text));
}

void WebClientBase::message_header(BufferRef&& name, BufferRef&& value)
{
	header(std::move(name), std::move(value));
}

bool WebClientBase::message_content(BufferRef&& chunk)
{
	content(std::move(chunk));

	return true;
}

bool WebClientBase::message_end()
{
	int pending = --request_count_;
	TRACE("message_end: pending=%d", pending);

	return complete();
}

// ---------------------------------------------------------------------------

WebClient::WebClient(struct ev_loop *loop) :
	WebClientBase(loop),
	on_connect(),
	on_response(),
	on_header(),
	on_content(),
	on_complete()
{
}

void WebClient::connect()
{
	if (on_connect)
		on_connect();
}

void WebClient::response(int major, int minor, int code, BufferRef&& text)
{
	if (on_response)
		on_response(major, minor, code, std::move(text));
}

void WebClient::header(BufferRef&& name, BufferRef&& value)
{
	if (on_header)
		on_header(std::move(name), std::move(value));
}

bool WebClient::content(BufferRef&& chunk)
{
	if (on_content)
		on_content(std::move(chunk));

	return true;
}

bool WebClient::complete()
{
	if (on_complete)
		return on_complete();

	return true;
}

} // namespace x0
