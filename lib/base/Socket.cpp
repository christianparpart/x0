/* <x0/Socket.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Socket.h>
#include <x0/SocketSpec.h>
#include <x0/Buffer.h>
#include <x0/Defines.h>
#include <x0/StackTrace.h>
#include <x0/DebugLogger.h>
#include <x0/io/Pipe.h>
#include <atomic>
#include <system_error>

#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#if !defined(XZERO_NDEBUG)
#	define TRACE(level, msg...) XZERO_DEBUG("Socket", (level), msg)
#else
#	define TRACE(msg...) do { } while (0)
#endif

#define ERROR(msg...) { \
    TRACE(1, msg); \
    TRACE(1, "Stack Trace:\n%s", StackTrace().c_str()); \
}

namespace x0 {

inline const char * mode_str(Socket::Mode m)
{
    static const char *ms[] = { "None", "Read", "Write", "ReadWrite" };
    return ms[static_cast<int>(m)];
}

Socket::Socket(struct ev_loop* loop) :
#ifndef XZERO_NDEBUG
    Logging(),
#endif
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
#ifndef XZERO_NDEBUG
//	setLogging(false);
    static std::atomic<unsigned long long> id(0);
    setLoggingPrefix("Socket(%d, %s:%d)", ++id, remoteIP().c_str(), remotePort());
#endif
    TRACE(1, "created. fd:%d, local(%s:%d)", fd_, localIP().c_str(), localPort());

    watcher_.set<Socket, &Socket::io>(this);
    timer_.set<Socket, &Socket::timeout>(this);
}

Socket::Socket(struct ev_loop* loop, int fd, int af, State state) :
    loop_(loop),
    watcher_(loop),
    timer_(loop),
    startedAt_(ev_now(loop)),
    lastActivityAt_(ev_now(loop)),
    fd_(fd),
    addressFamily_(af),
    secure_(false),
    state_(state),
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
#ifndef XZERO_NDEBUG
    setLogging(false);
    static std::atomic<unsigned long long> id(0);
    setLoggingPrefix("Socket(%d, %s:%d)", ++id, remoteIP().c_str(), remotePort());
#endif
    TRACE(1, "created. fd:%d, local(%s:%d)", fd_, localIP().c_str(), localPort());

    watcher_.set<Socket, &Socket::io>(this);
    timer_.set<Socket, &Socket::timeout>(this);
}

Socket::~Socket()
{
    TRACE(1, "destroying. fd:%d, local(%s:%d)", fd_, localIP().c_str(), localPort());

    close();
}

void Socket::set(int fd, int af)
{
    fd_ = fd;
    addressFamily_ = af;

    remotePort_ = 0;
    remoteIP_.clear();

    localPort_ = 0;
    localIP_.clear();
}

static inline bool applyFlags(int fd, int flags)
{
    return flags
        ? fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | flags) == 0
        : true;
}

Socket* Socket::open(struct ev_loop* loop, const SocketSpec& spec, int flags)
{
    int fd;
    State state = open(loop, spec, flags, &fd);
    if (state == Closed)
        return nullptr;

    Socket* socket = new Socket(loop, fd, spec.ipaddr().family(), state);

    if (state == Connecting)
        socket->setMode(Write);

    return socket;
}

Socket::State Socket::open(struct ev_loop* loop, const SocketSpec& spec, int flags, int* fd)
{
    // compute type-mask
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

    if (spec.isLocal()) {
        *fd = ::socket(PF_UNIX, SOCK_STREAM | typeMask, 0);
        if (*fd < 0)
            return Closed;

        if (!applyFlags(*fd, flags))
            return Closed;

        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        size_t addrlen = sizeof(addr.sun_family)
            + strlen(strncpy(addr.sun_path, spec.local().c_str(), sizeof(addr.sun_path)));

        if (::connect(*fd, (struct sockaddr*) &addr, addrlen) < 0) {
            ::close(*fd);
            *fd = -1;
            return Closed;
        }
        return Operational;
    } else {
        *fd = ::socket(spec.ipaddr().family(), SOCK_STREAM | typeMask, IPPROTO_TCP);
        if (*fd < 0)
            return Closed;

        if (!applyFlags(*fd, flags))
            return Closed;

        char buf[sizeof(sockaddr_in6)];
        std::size_t size;
        memset(&buf, 0, sizeof(buf));
        switch (spec.ipaddr().family()) {
            case IPAddress::V4:
                size = sizeof(sockaddr_in);
                ((sockaddr_in *)buf)->sin_port = htons(spec.port());
                ((sockaddr_in *)buf)->sin_family = AF_INET;
                memcpy(&((sockaddr_in *)buf)->sin_addr, spec.ipaddr().data(), spec.ipaddr().size());
                break;
            case IPAddress::V6:
                size = sizeof(sockaddr_in6);
                ((sockaddr_in6 *)buf)->sin6_port = htons(spec.port());
                ((sockaddr_in6 *)buf)->sin6_family = AF_INET6;
                memcpy(&((sockaddr_in6 *)buf)->sin6_addr, spec.ipaddr().data(), spec.ipaddr().size());
                break;
            default:
                ::close(*fd);
                *fd = -1;
                return Closed;
        }

        int rv = ::connect(*fd, (struct sockaddr*)buf, size);
        if (rv == 0) {
            return Operational;
        } else if (errno == EINPROGRESS) {
            return Connecting;
        } else {
            ::close(*fd);
            *fd = -1;
            return Closed;
        }
    }
}

bool Socket::open(const SocketSpec& spec, int flags)
{
    state_ = Socket::open(loop_, spec, flags, &fd_);
    return state_ != Closed;
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
    return setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
}

bool Socket::setTcpCork(bool enable)
{
#if defined(TCP_CORK)
    int flag = enable ? 1 : 0;
    bool rv = setsockopt(fd_, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) == 0;
    TRACE(1, "setTcpCork: %d => %d", enable, rv);
    tcpCork_ = rv ? enable : false;
    return rv;
#else
    return false;
#endif
}

TimeSpan Socket::lingering() const
{
    struct linger l;
    socklen_t sz = sizeof(l);
    if (getsockopt(fd_, SOL_SOCKET, SO_LINGER, &l, &sz) < 0)
        return TimeSpan::Zero;

    return TimeSpan::fromSeconds(l.l_linger);
}

bool Socket::setLingering(TimeSpan timeout)
{
    struct linger l;
    l.l_linger = timeout.value();
    l.l_onoff = l.l_linger > 0;

    return setsockopt(fd_, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == 0;
}

void Socket::setMode(Mode m)
{
    TRACE(1, "setMode() %s -> %s", mode_str(mode_), mode_str(m));
    if (isClosed())
        return;

    if (m != mode_) {
        if (m != None) {
            TRACE(1, "setMode: set flags");
            watcher_.set(fd_, static_cast<int>(m));

            if (!watcher_.is_active()) {
                TRACE(1, "setMode: start watcher");
                watcher_.start();
            }
        } else {
            TRACE(1, "stop watcher and timer");
            if (watcher_.is_active()) {
                watcher_.stop();
            }

            if (timer_.is_active()) {
                timer_.stop();
            }
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
    TRACE(1, "close: fd=%d (state:%s, timer_active:%s)", fd_, state_str(), timer_.is_active() ? "yes" : "no");

    if (timer_.is_active()) {
        TRACE(1, "close: stopping active timer");
        timer_.stop();
    }

    if (fd_< 0)
        return;

    state_ = Closed;
    mode_ = None;
    watcher_.stop();

    ::close(fd_);
    fd_ = -1;
}

ssize_t Socket::read(Buffer& result, size_t size)
{
    lastActivityAt_.update(ev_now(loop_));
    size_t nbytes = std::min(result.capacity() - result.size(), size);
    ssize_t rv = ::read(fd_, result.end(), nbytes);

    if (rv <= 0) {
        TRACE(1, "read(): rv=%ld: %s", rv, strerror(errno));
        return rv;
    }

    TRACE(1, "read() -> %ld", rv);
    result.resize(result.size() + rv);

    return rv;
}

ssize_t Socket::read(Buffer& result)
{
    ssize_t nread = 0;

    lastActivityAt_.update(ev_now(loop_));

    for (;;)
    {
        if (result.capacity() - result.size() < 256) {
            result.reserve(std::max(static_cast<std::size_t>(4096), static_cast<std::size_t>(result.size() * 1.5)));
        }

        size_t nbytes = result.capacity() - result.size();
        ssize_t rv = ::read(fd_, result.end(), nbytes);
        if (rv <= 0) {
            TRACE(1, "read(): rv=%ld -> %ld: %s", rv, result.size(), strerror(errno));
            return nread != 0 ? nread : rv;
        } else {
            TRACE(1, "read() -> %ld", rv);
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
            return rv;

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

#if 0 // !defined(XZERO_NDEBUG)
    //TRACE(1, "write('%s')", Buffer(buffer, size).c_str());
    ssize_t rv = ::write(fd_, buffer, size);
    TRACE(1, "write: %ld => %ld", size, rv);

    if (rv < 0 && errno != EINTR && errno != EAGAIN)
        ERROR("Socket(%d).write: error (%d): %s", fd_, errno, strerror(errno));

    return rv;
#else
    TRACE(1, "write(buffer, size=%ld)", size);
    return size ? ::write(fd_, buffer, size) : 0;
#endif
}

ssize_t Socket::write(int fd, off_t *offset, size_t nbytes)
{
    lastActivityAt_.update(ev_now(loop_));

    if (nbytes == 0)
        return 0;

    auto offset0 = *offset;
#ifdef __APPLE__
    ssize_t rv = ::sendfile(fd_, fd, offset, (off_t *) &nbytes, NULL, 0);
#else
    ssize_t rv = ::sendfile(fd_, fd, offset, nbytes);
#endif

#if !defined(XZERO_NDEBUG)
    TRACE(1, "write(fd=%d, offset=[%ld->%ld], nbytes=%ld) -> %ld", fd, offset0, *offset, nbytes, rv);

    if (rv < 0 && errno != EINTR && errno != EAGAIN)
        ERROR("Socket(%d).write(): sendfile: rv=%ld (%s)", fd_, rv, strerror(errno));
#else
    (void) offset0;
#endif

    return rv;
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
            TRACE(1, "onConnectComplete: connected");
            state_ = Operational;
        } else {
            TRACE(1, "onConnectComplete: error(%d): %s", val, strerror(val));
            close();
            errno = val;
        }
    } else {
        val = errno;
        TRACE(1, "onConnectComplete: getsocketopt() error: %s", strerror(errno));
        close();
        errno = val;
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

    TRACE(1, "io(revents=0x%04X): mode=%s", revents, mode_str(mode_));

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
    TRACE(1, "timeout(revents=0x%04X): mode=%d", revents, mode_);
    setMode(None);

    if (timeoutCallback_)
        timeoutCallback_(this, timeoutData_);
}

const IPAddress& Socket::remoteIP() const
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
                remoteIP_ = IPAddress(&saddr);
                remotePort_ = ntohs(saddr.sin6_port);
            }
            break;
        }
        case AF_INET: {
            sockaddr_in saddr;
            socklen_t slen = sizeof(saddr);
            if (getpeername(fd_, (sockaddr *)&saddr, &slen) == 0) {
                remoteIP_ = IPAddress(&saddr);
                remotePort_ = ntohs(saddr.sin_port);
            }
            break;
        }
        default:
            break;
    }
}

const IPAddress& Socket::localIP() const
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
                    localIP_ = IPAddress(&saddr);
                    localPort_ = ntohs(saddr.sin6_port);
                }
                break;
            }
            case AF_INET: {
                sockaddr_in saddr;
                socklen_t slen = sizeof(saddr);

                if (getsockname(fd_, (sockaddr *)&saddr, &slen) == 0) {
                    localIP_ = IPAddress(&saddr);
                    localPort_ = ntohs(saddr.sin_port);
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
