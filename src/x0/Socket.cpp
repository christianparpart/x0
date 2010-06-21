#include <x0/Socket.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/Defines.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>

#include <unistd.h>
#include <system_error>

namespace x0 {

Socket::Socket(struct ev_loop *loop, int fd) :
	loop_(loop),
	fd_(fd),
	watcher_(loop),
	timeout_(0),
	timer_(loop),
	mode_(IDLE),
	callback_(0),
	callbackData_(0)
{
	watcher_.set<Socket, &Socket::io>(this);
	timer_.set<Socket, &Socket::timeout>(this);
}

Socket::~Socket()
{
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

void Socket::setTimeout(int value)
{
	timeout_ = value;
}

void Socket::setMode(Mode m)
{
	if (m != mode_)
	{
		DEBUG("Socket(%d).setMode(%d)", fd_, m);

		static int modes[] = { 0, ev::READ, ev::WRITE };

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
		DEBUG("Socket(%d).read(): rv=%ld -> %ld:\n(%s)", fd_, rv, result.size(), result.substr(offset, rv).c_str());
	}
	else
	{
		DEBUG("Socket(%d).read(): rv=%ld (%s)", fd_, rv, strerror(errno));
	}

//	if (rv < 0)
//		return rv;

	//result.resize(result.size() + rv);

	return rv;
}

ssize_t Socket::write(const BufferRef& source)
{
	DEBUG("Socket(%d).write('%s')", fd_, source.str().c_str());
	return ::write(fd_, source.data(), source.size());
}

ssize_t Socket::write(int fd, off_t *offset, size_t nbytes)
{
#ifndef NDEBUG
	auto offset0 = *offset;
	ssize_t rv = ::sendfile(fd_, fd, offset, nbytes);
	DEBUG("Socket(%d).write(fd=%d, offset=[%ld->%ld], nbytes=%ld) -> %ld", fd_, fd, offset0, *offset, nbytes, rv);
	return rv;
#else
	return ::sendfile(fd_, fd, offset, nbytes);
#endif
}

void Socket::io(ev::io& io, int revents)
{
	DEBUG("Socket(%d).io(revents=0x%04X): mode=%d", fd_, revents, mode_);
	timer_.stop();

	if (callback_)
		callback_(this, callbackData_);
}

void Socket::timeout(ev::timer& timer, int revents)
{
	DEBUG("Socket(%d).timeout(revents=0x%04X): mode=%d", fd_, revents, mode_);
	watcher_.stop();

	if (callback_)
		callback_(this, callbackData_); // TODO pass arg to tell it timed out!
}

} // namespace x0
