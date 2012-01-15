/* <x0/Socket.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Socket.h>
#include <x0/SocketSpec.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/Defines.h>
#include <x0/StackTrace.h>
#include <x0/io/Pipe.h>
#include <atomic>
#include <system_error>

#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#if !defined(NDEBUG)
#	define TRACE(msg...) this->debug(msg)
#else
#	define TRACE(msg...) do { } while (0)
#endif

#define ERROR(msg...) { \
	TRACE(msg); \
	TRACE("Stack Trace:\n%s", StackTrace().c_str()); \
}

namespace x0 {

inline const char * mode_str(Socket::Mode m)
{
	static const char *ms[] = { "None", "Read", "Write", "ReadWrite" };
	return ms[static_cast<int>(m)];
}

Socket::Socket(struct ev_loop* loop) :
	loop_(loop),
	watcher_(loop),
	timer_(loop),
	startedAt_(ev_now(loop)),
	lastActivityAt_(ev_now(loop)),
	fd_(-1),
	addressFamily_(0),
	secure_(false),
	state_(Closed),
	mode_(None),
	tcpCork_(false),
	splicing_(true),
	remoteIP_(),
	remotePort_(0),
	localIP_(),
	localPort_(),
	callback_(nullptr),
	callbackData_(0)
{
#ifndef NDEBUG
	setLogging(false);
	static std::atomic<unsigned long long> id(0);
	setLoggingPrefix("Socket(%d, %s:%d)", ++id, remoteIP().c_str(), remotePort());
#endif
	TRACE("created. fd:%d, local(%s:%d)", fd_, localIP().c_str(), localPort());

	watcher_.set<Socket, &Socket::io>(this);
	timer_.set<Socket, &Socket::timeout>(this);
}

Socket::Socket(struct ev_loop* loop, int fd, int af) :
	loop_(loop),
	watcher_(loop),
	timer_(loop),
	startedAt_(ev_now(loop)),
	lastActivityAt_(ev_now(loop)),
	fd_(fd),
	addressFamily_(af),
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
	setLogging(false);
	static std::atomic<unsigned long long> id(0);
	setLoggingPrefix("Socket(%d, %s:%d)", ++id, remoteIP().c_str(), remotePort());
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

void Socket::set(int fd, int af)
{
	fd_ = fd;
	addressFamily_ = af;

	remoteIP_.clear();
	localIP_.clear();
}

bool Socket::openUnix(const std::string& unixPath, int flags)
{
#ifndef NDEBUG
	setLoggingPrefix("Socket(unix:%s)", unixPath.c_str());
#endif

	TRACE("connect(unix=%s)", unixPath.c_str());

	int typeMask = 0;

#if defined(SOCK_NONBLOCK)
	if (flags & O_NONBLOCK) {
		flags &= ~O_NONBLOCK;
		typeMask |= SOCK_NONBLOCK;
	}
#endif

#if defined(SOCK_CLOEXEC)
	if (flags & O_CLOEXEC) {
		flags &= ~O_CLOEXEC;
		typeMask |= SOCK_CLOEXEC;
	}
#endif

	fd_ = ::socket(PF_UNIX, SOCK_STREAM | typeMask, 0);
	if (fd_ < 0) {
		TRACE("socket creation error: %s",  strerror(errno));
		return false;
	}

	if (flags) {
		if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | flags) < 0) {
			// error
		}
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	size_t addrlen = sizeof(addr.sun_family)
		+ strlen(strncpy(addr.sun_path, unixPath.c_str(), sizeof(addr.sun_path)));

	int rv = ::connect(fd_, (struct sockaddr*) &addr, addrlen);
	if (rv < 0) {
		::close(fd_);
		fd_ = -1;
		TRACE("could not connect to %s: %s", unixPath.c_str(), strerror(errno));
		return false;
	}

	state_ = Operational;

	return true;
}

bool Socket::openTcp(const IPAddress& host, int port, int flags)
{
#ifndef NDEBUG
	setLoggingPrefix("Socket(tcp:%s:%d)", host.str().c_str(), port);
#endif

	int typeMask = 0;

#if defined(SOCK_NONBLOCK)
	if (flags & O_NONBLOCK) {
		flags &= ~O_NONBLOCK;
		typeMask |= SOCK_NONBLOCK;
	}
#endif

#if defined(SOCK_CLOEXEC)
	if (flags & O_CLOEXEC) {
		flags &= ~O_CLOEXEC;
		typeMask |= SOCK_CLOEXEC;
	}
#endif

	fd_ = ::socket(host.family(), SOCK_STREAM | typeMask, IPPROTO_TCP);
	if (fd_ < 0) {
		TRACE("socket creation error: %s",  strerror(errno));
		return false;
	}

	if (flags) {
		if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | flags) < 0) {
			// error
		}
	}

	int rv = ::connect(fd_, (struct sockaddr*) host.data(), host.size());
	if (rv == 0) {
		TRACE("connect: instant success (fd:%d)", fd_);
		state_ = Operational;
		return true;
	} else if (/*rv < 0 &&*/ errno == EINPROGRESS) {
		TRACE("connect: backgrounding (fd:%d)", fd_);
		state_ = Connecting;
		setMode(Write);
		return true;
	} else {
		TRACE("could not connect to %s:%d: %s", host.str().c_str(), port, strerror(errno));
		::close(fd_);
		fd_ = -1;
		return false;
	}
}

bool Socket::openTcp(const std::string& hostname, int port, int flags)
{
	TRACE("connect(hostname=%s, port=%d)", hostname.c_str(), port);

	struct addrinfo hints;
	struct addrinfo *res;
	bool result = false;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char sport[16];
	snprintf(sport, sizeof(sport), "%d", port);

	int rv = getaddrinfo(hostname.c_str(), sport, &hints, &res);
	if (rv) {
		TRACE("could not get addrinfo of %s:%s: %s", hostname.c_str(), sport, gai_strerror(rv));
		return false;
	}

	int typeMask = 0;
#if defined(SOCK_NONBLOCK)
	if (flags & O_NONBLOCK) {
		flags &= ~O_NONBLOCK;
		typeMask |= SOCK_NONBLOCK;
	}
#endif

#if defined(SOCK_CLOEXEC)
	if (flags & O_CLOEXEC) {
		flags &= ~O_CLOEXEC;
		typeMask |= SOCK_CLOEXEC;
	}
#endif

	for (struct addrinfo *rp = res; rp != nullptr; rp = rp->ai_next) {
		TRACE("creating socket(%d, %d, %d)", rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		fd_ = ::socket(rp->ai_family, rp->ai_socktype | typeMask, rp->ai_protocol);
		if (fd_ < 0) {
			TRACE("socket creation error: %s",  strerror(errno));
			continue;
		}

		if (flags) {
			if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | flags) < 0) {
				// error
			}
		}

		rv = ::connect(fd_, rp->ai_addr, rp->ai_addrlen);
		if (rv == 0) {
			TRACE("connect: instant success (fd:%d)", fd_);
			state_ = Operational;
			result = true;
			break;
		} else if (/*rv < 0 &&*/ errno == EINPROGRESS) {
			TRACE("connect: backgrounding (fd:%d)", fd_);

			state_ = Connecting;
			setMode(Write);

			result = true;
			break;
		} else {
			TRACE("could not connect to %s:%s: %s", hostname.c_str(), sport, strerror(errno));
			::close(fd_);
			fd_ = -1;
		}
	}

	freeaddrinfo(res);
	return result;
}

bool Socket::open(const SocketSpec& spec, int flags)
{
	if (spec.isLocal())
		return openUnix(spec.local, flags);
	else
		return openTcp(spec.address.str(), spec.port, flags);
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
	TRACE("setMode() %s -> %s", mode_str(mode_), mode_str(m));
	if (isClosed())
		return;

	if (m != mode_) {
		if (m != None) {
			TRACE("setMode: set flags");
			watcher_.set(fd_, static_cast<int>(m));

			if (!watcher_.is_active()) {
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

	state_ = Closed;
	mode_ = None;
	watcher_.stop();
	timer_.stop();

	::close(fd_);
	fd_ = -1;
}

ssize_t Socket::read(Buffer& result)
{
	ssize_t nread = 0;

	lastActivityAt_.update(ev_now(loop_));

	for (;;)
	{
		if (result.capacity() - result.size() < 256) {
			result.reserve(std::max(4096ul, static_cast<std::size_t>(result.size() * 1.5)));
		}

		size_t nbytes = result.capacity() - result.size();
		ssize_t rv = ::read(fd_, result.end(), nbytes);
		if (rv <= 0) {
			TRACE("read(): rv=%ld -> %ld: %s", rv, result.size(), strerror(errno));
			return nread != 0 ? nread : rv;
		} else {
			TRACE("read() -> %ld", rv);
			nread += rv;
			result.resize(result.size() + rv);

			if (static_cast<std::size_t>(rv) < nbytes) {
				return nread;
			}
		}
	}
}

ssize_t Socket::read(Pipe* pipe, size_t size)
{
	if (splicing())
	{
		ssize_t rv = pipe->write(fd_, size);

		if (rv >= 0)
			return 0;

		if (errno == ENOSYS)
			setSplicing(false);

		return rv;
	}

	// TODO fall back to classic userspace read()+write()
	errno = ENOSYS;
	return -1;
}

ssize_t Socket::write(const void *buffer, size_t size)
{
	lastActivityAt_.update(ev_now(loop_));

#if 0 // !defined(NDEBUG)
	//TRACE("write('%s')", Buffer(buffer, size).c_str());
	ssize_t rv = ::write(fd_, buffer, size);
	TRACE("write: %ld => %ld", size, rv);

	if (rv < 0 && errno != EINTR && errno != EAGAIN)
		ERROR("Socket(%d).write: error (%d): %s", fd_, errno, strerror(errno));

	return rv;
#else
	TRACE("write(buffer, size=%ld)", size);
	return size ? ::write(fd_, buffer, size) : 0;
#endif
}

ssize_t Socket::write(int fd, off_t *offset, size_t nbytes)
{
	lastActivityAt_.update(ev_now(loop_));

	if (nbytes == 0)
		return 0;

#if !defined(NDEBUG)
	auto offset0 = *offset;
	ssize_t rv = ::sendfile(fd_, fd, offset, nbytes);
	TRACE("write(fd=%d, offset=[%ld->%ld], nbytes=%ld) -> %ld", fd, offset0, *offset, nbytes, rv);

	if (rv < 0 && errno != EINTR && errno != EAGAIN)
		ERROR("Socket(%d).write(): sendfile: rv=%ld (%s)", fd_, rv, strerror(errno));

	return rv;
#else
	return ::sendfile(fd_, fd, offset, nbytes);
#endif
}

ssize_t Socket::write(Pipe* pipe, size_t size)
{
	if (splicing())
	{
		ssize_t rv = pipe->read(fd_, size);

		if (rv >= 0)
			return rv;

		if (errno == ENOSYS)
			setSplicing(false);

		return rv;
	}

	// TODO fall back to classic userspace read()+write()
	errno = ENOSYS;
	return -1;
}

void Socket::onConnectComplete()
{
	setMode(None);

	int val = 0;
	socklen_t vlen = sizeof(val);
	if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
		if (val == 0) {
			TRACE("onConnectComplete: connected");
			state_ = Operational;
		} else {
			TRACE("onConnectComplete: error(%d): %s", val, strerror(val));
			close();
		}
	} else {
		TRACE("onConnectComplete: getsocketopt() error: %s", strerror(errno));
		close();
	}

	if (callback_) {
		callback_(this, callbackData_, 0);
	}
}

void Socket::handshake(int /*revents*/)
{
	// plain (unencrypted) TCP/IP sockets do not need an additional handshake
	state_ = Operational;
}

void Socket::io(ev::io& /*io*/, int revents)
{
	lastActivityAt_.update(ev_now(loop_));

	TRACE("io(revents=0x%04X): mode=%s", revents, mode_str(mode_));

	timer_.stop();

	if (state_ == Connecting)
		onConnectComplete();
	else if (state_ == Handshake)
		handshake(revents);
	else if (callback_)
		callback_(this, callbackData_, revents);
}

void Socket::timeout(ev::timer& timer, int revents)
{
	TRACE("timeout(revents=0x%04X): mode=%d", revents, mode_);
	setMode(None);

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

std::string Socket::remote() const
{
	char buf[512];
	size_t n;
	switch (addressFamily_) {
		case AF_INET:
			n = snprintf(buf, sizeof(buf), "%s:%d", remoteIP().c_str(), remotePort());
			break;
		case AF_INET6:
			n = snprintf(buf, sizeof(buf), "[%s]:%d", remoteIP().c_str(), remotePort());
			break;
		default:
			n = snprintf(buf, sizeof(buf), "%s", remoteIP().c_str());
			break;
	}
	return std::string(buf, n);
}

std::string Socket::local() const
{
	char buf[512];
	size_t n;
	switch (addressFamily_) {
		case AF_INET:
			n = snprintf(buf, sizeof(buf), "%s:%d", localIP().c_str(), localPort());
			break;
		case AF_INET6:
			n = snprintf(buf, sizeof(buf), "[%s]:%d", localIP().c_str(), localPort());
			break;
		default:
			n = snprintf(buf, sizeof(buf), "%s", localIP().c_str());
			break;
	}
	return std::string(buf, n);
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

void Socket::inspect(Buffer& out)
{
	// only complain about potential bugs...

	out << "fd:" << fd_ << ", " << "timer:" << timer_.is_active() << "<br/>";

	if (watcher_.is_active() && mode_ != watcher_.events) {
		out << "<b>backend events differ from watcher mask</b><br/>";
	}

	out << "io.x0:" << mode_str(mode_);
	if (watcher_.is_active()) {
		out << ", io.ev:" << mode_str(static_cast<Mode>(watcher_.events));
	}
	out << "<br/>";

	struct stat st;
	if (fstat(fd_, &st) < 0) {
		out << "fd stat error: " << strerror(errno) << "<br/>";
	}
}

/** associates a new loop with this socket.
 *
 * \note this socket must not be active (in I/O and in timer)
 */
void Socket::setLoop(struct ev_loop* loop)
{
	// the socket must not be registered to the current loop
	assert(mode_ == None);
	assert(!watcher_.is_active());
	assert(!timer_.is_active());

	loop_ = loop;

	watcher_.set(loop);
	timer_.set(loop);
}

} // namespace x0
