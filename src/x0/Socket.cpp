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

#if !defined(NDEBUG)
#	include <x0/StackTrace.h>
#endif

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>

#include <unistd.h>
#include <system_error>

#define ERROR(msg...) { \
	DEBUG(msg); \
	StackTrace st; \
	DEBUG("Stack Trace:\n%s", st.c_str()); \
}

namespace x0 {

Socket::Socket(struct ev_loop *loop, int fd) :
	loop_(loop),
	fd_(fd),
	watcher_(loop),
	timeout_(0),
	timer_(loop),
	secure_(false),
	state_(OPERATIONAL),
	mode_(IDLE),
	callback_(0),
	callbackData_(0)
{
	//DEBUG("Socket(%p) fd=%d", this, fd_);

	watcher_.set<Socket, &Socket::io>(this);
	timer_.set<Socket, &Socket::timeout>(this);
}

Socket::~Socket()
{
	//DEBUG("~Socket(%p)", this);

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
	DEBUG("Socket(%d).setTcpCork: %d => %d", fd_, enable, rv);
	return rv;
#else
	return false;
#endif
}

void Socket::setMode(Mode m)
{
	if (m != mode_)
	{
		static int modes[] = { 0, ev::READ, ev::WRITE };
		//static const char *ms[] = { "null", "READ", "WRITE" };

		//DEBUG("Socket(%d).setMode(%s)", fd_, ms[static_cast<int>(m)]);

		watcher_.set(fd_, modes[static_cast<int>(m)]);

		if (mode_ == IDLE)
			watcher_.start();

		mode_ = m;
	}

	if (timeout_ > 0)
		timer_.start(timeout_, 0.0);
}

void Socket::clearReadyCallback()
{
	callback_ = NULL;
	callbackData_ = NULL;
}

void Socket::close()
{
	//DEBUG("Socket(%p).close: fd=%d", this, fd_);

	watcher_.stop();
	timer_.stop();

	::close(fd_);
	fd_ = -1;
}

ssize_t Socket::read(Buffer& result)
{
	std::size_t nbytes = result.capacity() - result.size();
	if (nbytes <= 0)
	{
		nbytes = 4096;
		result.reserve(result.size() + nbytes);
	}

	ssize_t rv = ::read(fd_, result.end(), nbytes);
	if (rv > 0)
	{
		auto offset = result.size();
		result.resize(offset + rv);
		//DEBUG("Socket(%d).read(): rv=%ld -> %ld:\n(%s)", fd_, rv, result.size(), result.substr(offset, rv).c_str());
	}
	else if (rv < 0 && errno != EINTR && errno != EAGAIN)
	{
		ERROR("Socket(%d).read(): rv=%ld (%s)", fd_, rv, strerror(errno));
	}

//	if (rv < 0)
//		return rv;

	//result.resize(result.size() + rv);

	return rv;
}

ssize_t Socket::write(const BufferRef& source)
{
#if !defined(NDEBUG)
	//DEBUG("Socket(%d).write('%s')", fd_, source.str().c_str());
	int rv = ::write(fd_, source.data(), source.size());

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
	//DEBUG("Socket(%d).write(fd=%d, offset=[%ld->%ld], nbytes=%ld) -> %ld", fd_, fd, offset0, *offset, nbytes, rv);

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
	//DEBUG("Socket(%d).io(revents=0x%04X): mode=%d", fd_, revents, mode_);
	timer_.stop();

	if (state_ == HANDSHAKE)
		handshake();
	else if (callback_)
		callback_(this, callbackData_);
}

void Socket::timeout(ev::timer& timer, int revents)
{
	//DEBUG("Socket(%d).timeout(revents=0x%04X): mode=%d", fd_, revents, mode_);
	watcher_.stop();

	if (timeoutCallback_)
		timeoutCallback_(this, timeoutData_);
}

} // namespace x0
