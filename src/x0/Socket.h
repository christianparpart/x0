#ifndef sw_x0_Socket_h
#define sw_x0_Socket_h (1)

#include <ev++.h>
#include <unistd.h>
#include <system_error>

namespace x0 {

class Buffer;
class BufferRef;

/** \brief represents a network socket.
 *
 * Features:
 * * nonblocking reads/writes,
 * * I/O and timeout event callbacks.
 */
class Socket
{
public:
	enum Mode {
		IDLE, READ, WRITE
	};

private:
	struct ev_loop *loop_;
	int fd_;
	ev::io watcher_;
	int timeout_;
	ev::timer timer_;
	Mode mode_;
	void (*callback_)(Socket *, void *);
	void *callbackData_;

public:
	explicit Socket(struct ev_loop *loop, int fd);
	virtual ~Socket();

	int handle() const;
	bool isClosed() const;

	bool setNonBlocking(bool enabled);
	bool setTcpNoDelay(bool enable);

	int timeout() const;
	void setTimeout(int value);

	Mode mode() const;
	void setMode(Mode m);
	template<class K, void (K::*cb)(Socket *)> void setReadyCallback(K *object);
	void clearReadyCallback();

	void close();

	// synchronous non-blocking I/O
	virtual ssize_t read(Buffer& result);
	virtual ssize_t write(const BufferRef& source);
	virtual ssize_t write(int fd, off_t *offset, size_t nbytes);

private:
	template<class K, void (K::*cb)(Socket *)>
	static void method_thunk(Socket *socket, void *object);

	void io(ev::io& io, int revents);
	void timeout(ev::timer& timer, int revents);
};

// {{{ inlines
inline int Socket::handle() const
{
	return fd_;
}

inline int Socket::timeout() const
{
	return timeout_;
}

inline Socket::Mode Socket::mode() const
{
	return mode_;
}

inline bool Socket::isClosed() const
{
	return fd_ < 0;
}

template<class K, void (K::*cb)(Socket *)>
inline void Socket::method_thunk(Socket *socket, void *object)
{
	(static_cast<K *>(object)->*cb)(socket);
}

template<class K, void (K::*cb)(Socket *)>
inline void Socket::setReadyCallback(K *object)
{
	callback_ = &method_thunk<K, cb>;
	callbackData_ = object;
}
// }}}

} // namespace x0

#endif
