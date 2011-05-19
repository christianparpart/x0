#include <x0/ServerSocket.h>
#include <x0/SocketDriver.h>
#include <x0/Socket.h>
#include <x0/IPAddress.h>
#include <x0/sysconfig.h>

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#include <sd-daemon.h>

namespace x0 {

/*!
 * \addtogroup base
 * \class ServerSocket
 * \brief represents a server listening socket (TCP/IP (v4 and v6) and Unix domain)
 *
 * \see Socket, SocketDriver
 */

ServerSocket::ServerSocket(struct ev_loop* loop) :
	loop_(loop),
	flags_(0),
	backlog_(SOMAXCONN),
	addressFamily_(AF_UNSPEC),
	fd_(-1),
	io_(loop),
	socketDriver_(new SocketDriver()),
	callback_(nullptr),
	callbackData_(nullptr)
{
}

ServerSocket::~ServerSocket()
{
	if (fd_ >= 0)
		::close(fd_);

	setSocketDriver(nullptr);
}

/*! sets the backlog for the listener socket.
 * \note must NOT be called if socket is already open.
 */
void ServerSocket::setBacklog(int value)
{
	assert(isOpen() == false);

	backlog_ = value;
}

/*! starts listening on a TCP/IP (v4 or v6) address.
 *
 * \param address the IP address to bind to
 * \param port the TCP/IP port number to listen to
 * \param flags some flags, such as O_CLOEXEC and O_NONBLOCK (the most prominent) to set on server socket and each created client socket
 *
 * \retval true successfully initialized
 * \retval false some failure occured during setting up the server socket.
 */
bool ServerSocket::open(const std::string& address, int port, int flags)
{
	int sd_fd_count = sd_listen_fds(false);
	addrinfo* res = nullptr;
	addrinfo hints;

	int rc;
	int fd = -1;
	in6_addr saddr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rc = inet_pton(AF_INET, address.c_str(), &saddr)) == 1) {
		hints.ai_family = AF_INET;
		hints.ai_flags |= AI_NUMERICHOST;
	} else if ((rc = inet_pton(AF_INET6, address.c_str(), &saddr)) == 1) {
		hints.ai_family = AF_INET6;
		hints.ai_flags |= AI_NUMERICHOST;
	} else {
		switch (rc) {
			case -1: // errno is set properly
				errorText_ = strerror(errno);
				break;
			case 0: // invalid network addr format
				errorText_ = strerror(EINVAL);
				break;
			case 1: // success
				errorText_ = strerror(0);
				break;
			default: // unknown error
				errorText_ = strerror(EINVAL);
				break;
		}
		return false;
	}

	char sport[16];
	snprintf(sport, sizeof(sport), "%d", port);

	if ((rc = getaddrinfo(address.c_str(), sport, &hints, &res)) != 0) {
		errorText_ = gai_strerror(rc);
		goto err;
	}

	typeMask_ = 0;

	if (flags & O_CLOEXEC) {
		flags &= ~O_CLOEXEC;
		typeMask_ |= SOCK_CLOEXEC;
	}

	if (flags & O_NONBLOCK) {
		flags &= ~O_NONBLOCK;
		typeMask_ |= SOCK_NONBLOCK;
	}

	flags_ = flags;

	// check if systemd created the socket for us
	if (sd_fd_count > 0) {
		fd = SD_LISTEN_FDS_START;
		int last = fd + sd_fd_count;

		for (addrinfo* ri = res; ri != nullptr; ri = ri->ai_next) {
			for (; fd < last; ++fd) {
				if (sd_is_socket_inet(fd, ri->ai_family, ri->ai_socktype, true, port) > 0) {
					// socket found, but ensure our expected `flags` are set.
					if (flags && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | flags) < 0) {
						goto err;
					} else {
						goto done;
					}
				}
			}
		}

		fprintf(stderr, "[ServerSocket] Running under systemd (socket unit file), but we received no socket for %s:%d.\n", address.c_str(), port);
		goto err;
	}

	// create socket manually
	for (addrinfo* ri = res; ri != nullptr; ri = ri->ai_next) {
		fd = socket(res->ai_family, res->ai_socktype | typeMask_, res->ai_protocol);
		if (fd < 0) {
			errorText_ = strerror(errno);
			goto err;
		}

		if (flags_ && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | flags_) < 0)
			goto err;

		rc = 1;

#if defined(SO_REUSEADDR)
		if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof(rc)) < 0)
			goto err;
#endif

#if defined(TCP_QUICKACK)
		if (::setsockopt(fd, SOL_TCP, TCP_QUICKACK, &rc, sizeof(rc)) < 0)
			goto err;
#endif

#if defined(TCP_DEFER_ACCEPT) && defined(WITH_TCP_DEFER_ACCEPT)
		if (::setsockopt(fd, SOL_TCP, TCP_DEFER_ACCEPT, &rc, sizeof(rc)) < 0)
			goto err;
#endif

		// TODO so_linger(false, 0)
		// TODO so_keepalive(true)

		if (::bind(fd, res->ai_addr, res->ai_addrlen) < 0)
			goto err;

		if (::listen(fd, backlog_) < 0)
			goto err;

		goto done;
	}

done:
	io_.set<ServerSocket, &ServerSocket::accept>(this);
	io_.set(fd, ev::READ);
	io_.start();

	fd_ = fd;
	addressFamily_ = res->ai_family;
	freeaddrinfo(res);
	address_ = address;
	port_ = port;

	return true;

err:
	if (res)
		freeaddrinfo(res);

	if (fd >= 0)
		::close(fd);

	return false;
}

/*! starts listening on a UNIX domain server socket.
 *
 * \param path the path on the local file system, that this socket is to listen to.
 * \param flags some flags, such as O_CLOEXEC and O_NONBLOCK (the most prominent) to set on server socket and each created client socket
 *
 * \retval true successfully initialized
 * \retval false some failure occured during setting up the server socket.
 */
bool ServerSocket::open(const std::string& path, int flags)
{
	int fd = -1;
	size_t addrlen;

	typeMask_ = 0;

	if (flags & O_CLOEXEC) {
		flags &= ~O_CLOEXEC;
		typeMask_ |= SOCK_CLOEXEC;
	}

	if (flags & O_NONBLOCK) {
		flags &= ~O_NONBLOCK;
		typeMask_ |= SOCK_NONBLOCK;
	}

	flags_ = flags;

	// check if systemd created the socket for us
	int count = sd_listen_fds(false);
	if (count > 0) {
		fd = SD_LISTEN_FDS_START;
		int last = fd + count;

		for (; fd < last; ++fd) {
			if (sd_is_socket_unix(fd, AF_UNIX, SOCK_STREAM, path.c_str(), path.size()) > 0) {
				// socket found, but ensure our expected `flags` are set.
				if (flags && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | flags) < 0) {
					goto err;
				} else {
					goto done;
				}
			}
		}

		fprintf(stderr, "[ServerSocket] Running under systemd (socket unit file), but we received no UNIX-socket for %s.\n", path.c_str());
		goto err;
	}

	// create socket manually
	fd = ::socket(PF_UNIX, SOCK_STREAM | typeMask_, 0);
	if (fd < 0)
		return false;

	if (flags_ && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | flags_) < 0)
		goto err;

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;

	if (path.size() >= sizeof(addr.sun_path)) {
		errno = ENAMETOOLONG;
		goto err;
	}

	addrlen = sizeof(addr.sun_family)
		+ strlen(strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)));

	if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), addrlen) < 0)
		goto err;

	if (::listen(fd, backlog_))
		goto err;

	if (chmod(path.c_str(), 0777) < 0) {
		perror("chmod");
	}

done:
	io_.set<ServerSocket, &ServerSocket::accept>(this);
	io_.set(fd, ev::READ);
	io_.start();

	fd_ = fd;
	addressFamily_ = AF_UNIX;
	address_ = path;
	port_ = 0;
	return true;

err:
	::close(fd);

	return false;
}

/*! stops listening and closes the server socket.
 *
 * \see open()
 */
void ServerSocket::close()
{
	if (fd_ < 0)
		return;

	if (addressFamily_ == AF_UNIX)
		unlink(address_.c_str());

	io_.stop();
	::close(fd_);
	fd_ = -1;
}

/*! defines a socket driver to be used for creating the client sockets.
 *
 * This is helpful when you want to create an SSL-aware server-socket, then set a custom socket driver, that is
 * creating subclassed SSL-aware Socket instances.
 *
 * See the ssl plugin as an example.
 */
void ServerSocket::setSocketDriver(SocketDriver* sd)
{
	if (socketDriver_ == sd)
		return;

	if (socketDriver_)
		delete socketDriver_;

	socketDriver_ = sd;
}

void ServerSocket::accept(ev::io&, int)
{
	Socket* cs = nullptr;
	int cfd;

#if defined(HAVE_ACCEPT4) && defined(WITH_ACCEPT4)
	bool flagged = true;
	cfd = ::accept4(fd_, nullptr, 0, typeMask_);
	if (cfd < 0 && errno == ENOSYS) {
		cfd = ::accept(fd_, nullptr, 0);
		flagged = false;
	}
#else
	bool flagged = false;
	cfd = ::accept(fd_, nullptr, 0);
#endif

	if (cfd < 0) {
		switch (errno) {
			case EINTR:
			case EAGAIN:
#if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#endif
				return;
			default:
				goto done;
		}
	}

	if (!flagged && flags_ && fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL) | flags_) < 0)
		goto err;

	cs = socketDriver_->create(loop_, cfd, addressFamily_);
	goto done;

err:
	::close(cfd);

done:
	assert(callback_ != nullptr);
	callback_(cs, this);
}

} // namespace x0
