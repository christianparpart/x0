/* <x0/Socket.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Socket.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/Defines.h>
#include <x0/StackTrace.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>

#include <unistd.h>
#include <system_error>

#if 1 // !defined(NDEBUG)
#	define TRACE(msg...) this->debug(msg)
#else
#	define TRACE(msg...) ((void *)0)
#endif

#define ERROR(msg...) { \
	TRACE(msg); \
	TRACE("Stack Trace:\n%s", StackTrace().c_str()); \
}

namespace x0 {

Socket::Socket(struct ev_loop *loop, int fd, int af) :
	loop_(loop),
	fd_(fd),
	addressFamily_(af),
	watcher_(loop),
	timer_(loop),
	secure_(false),
	state_(Operational),
	mode_(None),
	tcpCork_(false),
	remoteIP_(),
	remotePort_(0),
	localIP_(),
	localPort_(),
	callback_(nullptr),
	callbackData_(0)
{
#ifndef NDEBUG
	setLoggingPrefix("Socket(%s:%d)", remoteIP().c_str(), remotePort());
#endif
	TRACE("created. fd:%d, local(%s:%d)", fd_, localIP().c_str(), localPort());

	watcher_.set<Socket, &Socket::io>(this);
	timer_.set<Socket, &Socket::timeout>(this);
}

Socket::~Socket()
{
	TRACE("destroying. fd:%d, local(%s:%d)", fd_, localIP().c_str(), localPort());

	if (fd_ >= 0)
		::close(fd_);
}

bool Socket::setNonBlocking(bool enabled)
{
	if (enabled)
		return fcntl(fd_, F_SETFL, O_NONBLOCK) == 0;
	else
		return fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) & ~O_NONBLOCK) == 0;
}

bool Socket::setTcpNoDelay(bool enable)
{
	int flag = enable ? 1 : 0;
	return setsockopt(fd_, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
}

bool Socket::setTcpCork(bool enable)
{
#if defined(TCP_CORK)
	int flag = enable ? 1 : 0;
	bool rv = setsockopt(fd_, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) == 0;
	TRACE("setTcpCork: %d => %d", enable, rv);
	tcpCork_ = rv ? enable : false;
	return rv;
#else
	return false;
#endif
}

void Socket::setMode(Mode m)
{
	static const char *ms[] = { "None", "Read", "Write", "ReadWrite" };
	TRACE("setMode() %s -> %s", ms[static_cast<int>(mode_)], ms[static_cast<int>(m)]);

	if (m != mode_) {
		if (m != None) {
			TRACE("setMode: set flags");
			watcher_.set(fd_, static_cast<int>(m));

			if (mode_ == None && !watcher_.is_active()) {
				TRACE("setMode: start watcher");
				watcher_.start();
			}
		} else {
			TRACE("stop watcher and timer");
			if (watcher_.is_active())
				watcher_.stop();

			if (timer_.is_active())
				timer_.stop();
		}

		mode_ = m;
	}
}

void Socket::clearReadyCallback()
{
	callback_ = nullptr;
	callbackData_ = nullptr;
}

void Socket::close()
{
	TRACE("close: fd=%d", fd_);

	if (fd_< 0)
		return;

	watcher_.stop();
	timer_.stop();

	::close(fd_);
	fd_ = -1;
}

ssize_t Socket::read(Buffer& result)
{
	ssize_t nread = 0;

	for (;;)
	{
		if (result.capacity() - result.size() < 256)
			result.reserve(result.size() * 1.5);

		ssize_t rv = ::read(fd_, result.end(), result.capacity() - result.size());
		if (rv <= 0) {
			TRACE("read(): rv=%ld -> %ld: %s", rv, result.size(), strerror(errno));
			return nread != 0 ? nread : rv;
		} else {
			TRACE("read() -> %ld", rv);
			nread += rv;
			result.resize(result.size() + rv);
		}
	}
}

ssize_t Socket::write(const void *buffer, size_t size)
{
#if 0 // !defined(NDEBUG)
	//TRACE("write('%s')", source.str().c_str());
	ssize_t rv = ::write(fd_, source.data(), source.size());
	TRACE("write: %ld => %ld", source.size(), rv);

	if (rv < 0 && errno != EINTR && errno != EAGAIN)
		ERROR("Socket(%d).write: error (%d): %s", fd_, errno, strerror(errno));

	return rv;
#else
	return ::write(fd_, buffer, size);
#endif
}

ssize_t Socket::write(int fd, off_t *offset, size_t nbytes)
{
#if !defined(NDEBUG)
	//auto offset0 = *offset;
	ssize_t rv = ::sendfile(fd_, fd, offset, nbytes);
	//TRACE("write(fd=%d, offset=[%ld->%ld], nbytes=%ld) -> %ld", fd, offset0, *offset, nbytes, rv);

	if (rv < 0 && errno != EINTR && errno != EAGAIN)
		ERROR("Socket(%d).write(): sendfile: rv=%ld (%s)", fd_, rv, strerror(errno));

	return rv;
#else
	return ::sendfile(fd_, fd, offset, nbytes);
#endif
}

void Socket::handshake(int /*revents*/)
{
	// plain (unencrypted) TCP/IP sockets do not need an additional handshake
}

void Socket::io(ev::io& /*io*/, int revents)
{
	//TRACE("io(revents=0x%04X): mode=%d", revents, mode_);
	timer_.stop();

	if (state_ == Handshake)
		handshake(revents);
	else if (callback_)
		callback_(this, callbackData_, revents);
}

void Socket::timeout(ev::timer& timer, int revents)
{
	TRACE("timeout(revents=0x%04X): mode=%d", revents, mode_);
	watcher_.stop();

	if (timeoutCallback_)
		timeoutCallback_(this, timeoutData_);
}

std::string Socket::remoteIP() const
{
	const_cast<Socket *>(this)->queryRemoteName();
	return remoteIP_;
}

unsigned int Socket::remotePort() const
{
	const_cast<Socket *>(this)->queryRemoteName();
	return remotePort_;
}


void Socket::queryRemoteName()
{
	if (remotePort_ || fd_ < 0)
		return;

	switch (addressFamily_) {
		case AF_INET6: {
			sockaddr_in6 saddr;
			socklen_t slen = sizeof(saddr);
			if (getpeername(fd_, (sockaddr *)&saddr, &slen) == 0) {
				char buf[128];

				if (inet_ntop(AF_INET6, &saddr.sin6_addr, buf, sizeof(buf))) {
					remoteIP_ = buf;
					remotePort_ = ntohs(saddr.sin6_port);
				}
			}
			break;
		}
		case AF_INET: {
			sockaddr_in saddr;
			socklen_t slen = sizeof(saddr);
			if (getpeername(fd_, (sockaddr *)&saddr, &slen) == 0) {
				char buf[128];

				if (inet_ntop(AF_INET, &saddr.sin_addr, buf, sizeof(buf))) {
					remoteIP_ = buf;
					remotePort_ = ntohs(saddr.sin_port);
				}
			}
			break;
		}
		default:
			break;
	}
}

std::string Socket::localIP() const
{
	const_cast<Socket *>(this)->queryLocalName();
	return localIP_;
}

unsigned int Socket::localPort() const
{
	const_cast<Socket *>(this)->queryLocalName();
	return localPort_;
}

void Socket::queryLocalName()
{
	if (!localPort_ && fd_ >= 0) {
		switch (addressFamily_) {
			case AF_INET6: {
				sockaddr_in6 saddr;
				socklen_t slen = sizeof(saddr);

				if (getsockname(fd_, (sockaddr *)&saddr, &slen) == 0) {
					char buf[128];

					if (inet_ntop(AF_INET6, &saddr.sin6_addr, buf, sizeof(buf))) {
						localIP_ = buf;
						localPort_ = ntohs(saddr.sin6_port);
					}
				}
				break;
			}
			case AF_INET: {
				sockaddr_in saddr;
				socklen_t slen = sizeof(saddr);

				if (getsockname(fd_, (sockaddr *)&saddr, &slen) == 0) {
					char buf[128];

					if (inet_ntop(AF_INET, &saddr.sin_addr, buf, sizeof(buf))) {
						localIP_ = buf;
						localPort_ = ntohs(saddr.sin_port);
					}
				}
				break;
			}
			default:
				break;
		}
	}
}

} // namespace x0
