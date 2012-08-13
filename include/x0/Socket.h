/* <Socket.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_Socket_h
#define sw_x0_Socket_h (1)

#include <x0/IPAddress.h>
#include <x0/TimeSpan.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/Logging.h>
#include <x0/DateTime.h>

#include <ev++.h>
#include <unistd.h>
#include <system_error>

namespace x0 {

class SocketSpec;
class Pipe;

/** \brief represents a network socket.
 *
 * Features:
 * * nonblocking reads/writes,
 * * I/O and timeout event callbacks.
 */
class X0_API Socket
#ifndef NDEBUG
	: public Logging
#endif
{
public:
	enum Mode {
		None = ev::NONE,
		Read = ev::READ,
		Write = ev::WRITE,
		ReadWrite = ev::READ | ev::WRITE
	};

	enum State {
		Closed,
		Connecting,
		Handshake,
		Operational
	};

private:
	struct ev_loop *loop_;
	ev::io watcher_;
	ev::timer timer_;
	DateTime startedAt_;
	DateTime lastActivityAt_;
	int fd_;
	int addressFamily_;
	bool secure_;
	State state_;
	Mode mode_;
	bool tcpCork_;
	bool splicing_;

	mutable std::string remoteIP_;		//!< internal cache to remote ip
	mutable unsigned int remotePort_;	//!< internal cache to remote port

	mutable std::string localIP_;		//!< internal cache to local ip
	mutable unsigned int localPort_;	//!< internal cache to local port

	void (*callback_)(Socket *, void *, int);
	void *callbackData_;

	void (*timeoutCallback_)(Socket *, void *);
	void *timeoutData_;

protected:
	void (*handshakeCallback_)(Socket *, void *);
	void *handshakeData_;

public:
	explicit Socket(struct ev_loop *loop);
	Socket(struct ev_loop *loop, int fd, int addressFamily);
	virtual ~Socket();

	const DateTime& startedAt() const { return startedAt_; }
	const DateTime& lastActivityAt() const { return lastActivityAt_; }

	void set(int fd, int addressFamily);

	bool openUnix(const std::string& unixPath, int flags = 0);
	bool openTcp(const std::string& hostname, int port, int flags = 0);
	bool openTcp(const IPAddress& host, int port, int flags = 0);
	bool open(const SocketSpec& spec, int flags = 0);

	int handle() const;
	bool isOpen() const;
	bool isClosed() const;

	bool isSecure() const;
	void setSecure(bool enabled);

	bool setNonBlocking(bool enabled);
	bool setTcpNoDelay(bool enable);

	bool tcpCork() const;
	bool setTcpCork(bool enable);

	bool splicing() const;
	void setSplicing(bool enable);

	std::string remoteIP() const;
	unsigned int remotePort() const;
	std::string remote() const;

	std::string localIP() const;
	unsigned int localPort() const;
	std::string local() const;

	template<class K, void (K::*cb)(Socket *)> void setTimeout(K *object, TimeSpan value);

	const char *state_str() const;
	State state() const;
	void setState(State s);

	Mode mode() const;
	void setMode(Mode m);

	void setTimeout(TimeSpan value);
	bool timerActive() const { return timer_.is_active(); }

	template<class K, void (K::*cb)(Socket *, int)> void setReadyCallback(K *object);
	void clearReadyCallback();

	// initiates the handshake
	template<class K, void (K::*cb)(Socket *)> void handshake(K *object);

	void close();

	struct ev_loop* loop() const;
	void setLoop(struct ev_loop* loop);

	// synchronous non-blocking I/O
	virtual ssize_t read(Buffer& result);
	virtual ssize_t read(Pipe* buffer, size_t size);
	virtual ssize_t write(int fd, off_t *offset, size_t nbytes);
	virtual ssize_t write(const void *buffer, size_t size);
	virtual ssize_t write(Pipe* buffer, size_t size);

	ssize_t write(const BufferRef& source);
	template<typename PodType, std::size_t N> ssize_t write(PodType (&value)[N]);

	virtual void inspect(Buffer& out);

private:
	void queryRemoteName();
	void queryLocalName();

	template<class K, void (K::*cb)(Socket *, int)>
	static void io_thunk(Socket *socket, void *object, int revents);

	template<class K, void (K::*cb)(Socket *)>
	static void member_thunk(Socket *socket, void *object);

	void io(ev::io& io, int revents);
	void timeout(ev::timer& timer, int revents);

protected:
	void onConnectComplete();
	virtual void handshake(int revents);

	void callback(int revents);
};

// {{{ inlines
inline int Socket::handle() const
{
	return fd_;
}

inline const char *Socket::state_str() const
{
	switch (state_)
	{
		case Closed:
			return "CLOSED";
		case Connecting:
			return "CONNECTING";
		case Handshake:
			return "HANDSHAKE";
		case Operational:
			return "OPERATIONAL";
		default:
			return "<INVALID>";
	}
}

inline Socket::State Socket::state() const
{
	return state_;
}

inline void Socket::setState(State s)
{
	state_ = s;
}

inline Socket::Mode Socket::mode() const
{
	return mode_;
}

inline bool Socket::tcpCork() const
{
	return tcpCork_;
}

inline bool Socket::splicing() const
{
	return splicing_;
}

inline void Socket::setSplicing(bool enable)
{
	splicing_ = enable;
}

inline bool Socket::isOpen() const
{
	return fd_ >= 0;
}

inline bool Socket::isClosed() const
{
	return fd_ < 0;
}

inline bool Socket::isSecure() const
{
	return secure_;
}

inline void Socket::setSecure(bool enabled)
{
	secure_ = enabled;
}

template<class K, void (K::*cb)(Socket *, int)>
inline void Socket::io_thunk(Socket *socket, void *object, int revents)
{
	(static_cast<K *>(object)->*cb)(socket, revents);
}

template<class K, void (K::*cb)(Socket *)>
inline void Socket::member_thunk(Socket *socket, void *object)
{
	(static_cast<K *>(object)->*cb)(socket);
}

template<class K, void (K::*cb)(Socket *, int)>
inline void Socket::setReadyCallback(K *object)
{
	callback_ = &io_thunk<K, cb>;
	callbackData_ = object;
}

template<class K, void (K::*cb)(Socket *)>
inline void Socket::setTimeout(K *object, TimeSpan value)
{
#if !defined(NDEBUG)
	debug("setTimeout(%p, %d) active:%s", object, value(), timer_.is_active() ? "true" : "false");
#endif

	timeoutCallback_ = &member_thunk<K, cb>;
	timeoutData_ = object;

	if (timer_.is_active())
		timer_.stop();

	if (value)
		timer_.start(value(), 0.0);
}

template<class K, void (K::*cb)(Socket *)>
inline void Socket::handshake(K *object)
{
	handshakeCallback_ = &member_thunk<K, cb>;
	handshakeData_ = object;

	handshake(None);
}

/** invokes I/O-activity callback. */
inline void Socket::callback(int revents)
{
	if (callback_)
		callback_(this, callbackData_, revents);
}

inline struct ev_loop* Socket::loop() const
{
	return loop_;
}

inline ssize_t Socket::write(const BufferRef& source)
{
	return write(source.data(), source.size());
}

template<typename PodType, std::size_t N>
inline ssize_t Socket::write(PodType (&value)[N])
{
	return write(value, N - 1);
}
// }}}

} // namespace x0

#endif
