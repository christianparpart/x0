/* <HttpConnection.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_h
#define x0_connection_h (1)

#include <x0/http/HttpMessageProcessor.h>
#include <x0/http/HttpWorker.h>
#include <x0/io/CompositeSource.h>
#include <x0/io/SocketSink.h>
#include <x0/CustomDataMgr.h>
#include <x0/TimeSpan.h>
#include <x0/Socket.h>
#include <x0/Buffer.h>
#include <x0/Property.h>
#include <x0/Logging.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <x0/sysconfig.h>

#include <functional>
#include <memory>
#include <string>
#include <ev++.h>

namespace x0 {

//! \addtogroup http
//@{

class HttpRequest;

/**
 * @brief HTTP client connection object.
 * @see HttpRequest, HttpServer
 */
class X0_API HttpConnection :
#ifndef NDEBUG
	public Logging,
#endif
	public HttpMessageProcessor,
	public CustomDataMgr
{
public:
	HttpConnection& operator=(const HttpConnection&) = delete;
	HttpConnection(const HttpConnection&) = delete;

	void ref();
	void unref();

	/**
	 * creates an HTTP connection object.
	 * \param srv a ptr to the server object this connection belongs to.
	 */
	HttpConnection(HttpWorker* worker, unsigned long long id);

	~HttpConnection();

	unsigned long long id() const;				//!< returns the (mostly) unique, worker-local, ID to this connection

	void close();

	ValueProperty<bool> secure;					//!< true if this is a secure (HTTPS) connection, false otherwise.

	Socket *socket() const;						//!< Retrieves a pointer to the connection socket.
	HttpWorker& worker() const;					//!< Retrieves a reference to the owning worker.

	std::string remoteIP() const;				//!< Retrieves the IP address of the remote end point (client).
	unsigned int remotePort() const;			//!< Retrieves the TCP port numer of the remote end point (client).

	std::string localIP() const;
	unsigned int localPort() const;

	const HttpListener& listener() const;

	bool isSecure() const;

	void write(Source* buffer);
	template<class T, class... Args> void write(Args&&... args);

	bool isOutputPending() const;

	const HttpRequest* request() const { return request_; }
	HttpRequest* request() { return request_; }

private:
	friend class HttpRequest;
	friend class HttpListener;
	friend class HttpWorker;

	// overrides from HttpMessageProcessor:
	virtual void messageBegin(BufferRef&& method, BufferRef&& entity, int version_major, int version_minor);
	virtual void messageHeader(BufferRef&& name, BufferRef&& value);
	virtual bool messageHeaderEnd();
	virtual bool messageContent(BufferRef&& chunk);
	virtual bool messageEnd();

	void start(HttpListener* listener, int fd, const HttpConnectionList::iterator& handle);
	void resume();

	bool isAborted() const;
	bool isClosed() const;

	void handshakeComplete(Socket *);

	void watchInput(const TimeSpan& timeout = TimeSpan::Zero);
	void watchOutput();

	void processInput();
	void processOutput();

	void process();
	void io(Socket *socket, int revents);
	void timeout(Socket *socket);

	struct ::ev_loop *loop() const;

	void abort();
	void checkFinish();

	void log(Severity s, const char *fmt, ...);

	Buffer& inputBuffer() { return buffer_; }
	const Buffer& inputBuffer() const { return buffer_; }

private:
	unsigned refCount_;
	HttpListener* listener_;
	HttpWorker* worker_;
	HttpConnectionList::iterator handle_;

	Socket* socket_;					//!< underlying communication socket
	unsigned long long id_;				//!< the worker-local connection-ID
	unsigned requestCount_;				//!< the number of requests already processed or currently in process
	enum State { Alive, Aborted, Closed } state_;
	bool isHandlingRequest_;			//!< is this connection (& request) currently passed to a request handler?

	// HTTP HttpRequest
	Buffer buffer_;						//!< buffer for incoming data.
	std::size_t offset_;				//!< number of bytes in buffer_ successfully processed already.
	HttpRequest* request_;				//!< currently parsed http HttpRequest, may be NULL

	void (*abortHandler_)(void*);
	void* abortData_;

	CompositeSource source_;
	SocketSink sink_;
};

// {{{ inlines
inline struct ::ev_loop* HttpConnection::loop() const
{
	return worker_->loop();
}

inline Socket* HttpConnection::socket() const
{
	return socket_;
}

inline unsigned long long HttpConnection::id() const
{
	return id_;
}

inline HttpWorker& HttpConnection::worker() const
{
	return *worker_;
}

template<class T, class... Args>
inline void HttpConnection::write(Args&&... args)
{
	if (!isAborted()) {
		write(new T(args...));
	}
}

inline const HttpListener& HttpConnection::listener() const
{
	return *listener_;
}

/*! Tests whether if the connection to the client (remote end-point) has * been aborted early.
 */
inline bool HttpConnection::isAborted() const
{
	return state_ != Alive; // Aborted or Closed
}

inline bool HttpConnection::isClosed() const
{
	return state_ == Closed;
}

/*! Tests whether or not this connection has pending data to sent to the client.
 */
inline bool HttpConnection::isOutputPending() const
{
	return !source_.empty();
}
// }}}

//@}

} // namespace x0

#endif
