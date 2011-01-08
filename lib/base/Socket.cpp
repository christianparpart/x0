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

#if 0 // !defined(NDEBUG)
#	define TRACE(msg...) DEBUG("Socket: " msg)
#else
#	define TRACE(msg...)
#endif

#define ERROR(msg...) { \
	TRACE(msg); \
	StackTrace st; \
	TRACE("Stack Trace:\n%s", st.c_str()); \
}

namespace x0 {

Socket::Socket(struct ev_loop *loop, int fd, int af) :
	loop_(loop),
	fd_(fd),
	addressFamily_(af),
	watcher_(loop),
	timeout_(0),
	timer_(loop),
	secure_(false),
	state_(OPERATIONAL),
	mode_(IDLE),
	tcpCork_(false),
	remoteIP_(),
	remotePort_(0),
	localIP_(),
	localPort_(),
	callback_(0),
	callbackData_(0)
{
	TRACE("Socket(%p) fd:%d, local(%s:%d), remote(%s:%d)", this, fd_, localIP().c_str(), localPort(), remoteIP().c_str(), remotePort());

	watcher_.set<Socket, &Socket::io>(this);
	timer_.set<Socket, &Socket::timeout>(this);
}

Socket::~Socket()
{
	TRACE("~Socket(%p) fd:%d, local(%s:%d), remote(%s:%d)", this, fd_, localIP().c_str(), localPort(), remoteIP().c_str(), remotePort());

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
	TRACE("(%d).setTcpCork: %d => %d", fd_, enable, rv);
	tcpCork_ = rv ? enable : false;
	return rv;
#else
	return false;
#endif
}

void Socket::setMode(Mode m)
{
	switch (m) {
	case READ:
	case WRITE:
		if (m != mode_)
		{
			static int modes[] = { 0, ev::READ, ev::WRITE };
			//static const char *ms[] = { "null", "READ", "WRITE" };

			//TRACE("(%d).setMode(%s)", fd_, ms[static_cast<int>(m)]);

			watcher_.set(fd_, modes[static_cast<int>(m)]);

			if (mode_ == IDLE)
				watcher_.start();
		}

		if (timeout_ > 0)
			timer_.start(timeout_, 0.0);

		break;
	case IDLE:
		if (watcher_.is_active())
			watcher_.stop();

		if (timer_.is_active())
			timer_.stop();

		break;
	}

	mode_ = m;
}

void Socket::clearReadyCallback()
{
	callback_ = NULL;
	callbackData_ = NULL;
}

void Socket::close()
{
	TRACE("(%p).close: fd=%d", this, fd_);

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
			TRACE("(%d).read(): rv=%ld -> %ld:\n", fd_, rv, result.size());
			return nread != 0 ? nread : rv;
		} else {
			nread += rv;
			size_t offset = result.size();
			result.resize(offset + rv);
		}
	}
}

ssize_t Socket::write(const BufferRef& source)
{
#if 0 // !defined(NDEBUG)
	//TRACE("(%d).write('%s')", fd_, source.str().c_str());
	ssize_t rv = ::write(fd_, source.data(), source.size());
	TRACE("(%d).write: %ld => %ld", fd_, source.size(), rv);

	if (rv < 0 && errno != EINTR && errno != EAGAIN)
		ERROR("Socket(%d).write: error (%d): %s", fd_, errno, strerror(errno));

	return rv;
#else
	return ::write(fd_, source.data(), source.size());
#endif
}

ssize_t Socket::write(int fd, off_t *offset, size_t nbytes)
{
#if !defined(NDEBUG)
	//auto offset0 = *offset;
	ssize_t rv = ::sendfile(fd_, fd, offset, nbytes);
	//TRACE("(%d).write(fd=%d, offset=[%ld->%ld], nbytes=%ld) -> %ld", fd_, fd, offset0, *offset, nbytes, rv);

	if (rv < 0 && errno != EINTR && errno != EAGAIN)
		ERROR("Socket(%d).write(): sendfile: rv=%ld (%s)", fd_, rv, strerror(errno));

	return rv;
#else
	return ::sendfile(fd_, fd, offset, nbytes);
#endif
}

void Socket::handshake()
{
	// plain (unencrypted) TCP/IP sockets do not need an additional handshake
}

void Socket::io(ev::io& io, int revents)
{
	//TRACE("(%d).io(revents=0x%04X): mode=%d", fd_, revents, mode_);
	timer_.stop();

	if (state_ == HANDSHAKE)
		handshake();
	else if (callback_)
		callback_(this, callbackData_);
}

void Socket::timeout(ev::timer& timer, int revents)
{
	//TRACE("(%d).timeout(revents=0x%04X): mode=%d", fd_, revents, mode_);
	watcher_.stop();

	if (timeoutCallback_)
		timeoutCallback_(this, timeoutData_);
}

#if 1 == 0
bool Socket::acceptFrom(int listenerSocket)
{
	socklen_t slen = sizeof(saddr);
	int fd = ::accept(listenerSocket, reinterpret_cast<sockaddr *>(&saddr), &slen);
	if (fd < 0)
		return false;
}
#endif

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
