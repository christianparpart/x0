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
	enum Status {
		StartingUp,
		ReadingRequest,
		SendingReply,
		KeepAliveRead
	};

public:
	HttpConnection& operator=(const HttpConnection&) = delete;
	HttpConnection(const HttpConnection&) = delete;

	/**
	 * creates an HTTP connection object.
	 * \param srv a ptr to the server object this connection belongs to.
	 */
	HttpConnection(HttpWorker* worker, unsigned long long id);

	~HttpConnection();

	unsigned long long id() const;				//!< returns the (mostly) unique, worker-local, ID to this connection

	Status status() const { return status_; }
	const char* status_str() const;

	void close();

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
	bool isHandlingRequest() const;

	const HttpRequest* request() const { return request_; }
	HttpRequest* request() { return request_; }

	std::size_t inputSize() const { return input_.size(); }
	std::size_t inputOffset() const { return inputOffset_; }

private:
	friend class HttpRequest;
	friend class HttpListener;
	friend class HttpWorker;

	// overrides from HttpMessageProcessor:
	virtual bool onMessageBegin(const BufferRef& method, const BufferRef& entity, int versionMajor, int versionMinor);
	virtual bool onMessageHeader(const BufferRef& name, const BufferRef& value);
	virtual bool onMessageHeaderEnd();
	virtual bool onMessageContent(const BufferRef& chunk);
	virtual bool onMessageEnd();

	void ref();
	void unref();

	void start(HttpListener* listener, int fd, const HttpConnectionList::iterator& handle);
	void resume();
	void processResume();

	bool isAborted() const;
	bool isClosed() const;

	void handshakeComplete(Socket *);

	void watchInput(const TimeSpan& timeout = TimeSpan::Zero);
	void watchOutput();

	bool processInput();
	bool processOutput();

	void process();
	void io(Socket *socket, int revents);
	void timeout(Socket *socket);

	struct ::ev_loop *loop() const;

	void abort();

	void log(Severity s, const char *fmt, ...);

	Buffer& inputBuffer() { return input_; }
	const Buffer& inputBuffer() const { return input_; }

	void setShouldKeepAlive(bool enabled);
	bool shouldKeepAlive() const { return flags_ & IsKeepAliveEnabled; }

	bool isInsideSocketCallback() const { return refCount_ > 0; }

private:
	unsigned refCount_;

	Status status_;

	HttpListener* listener_;
	HttpWorker* worker_;
	HttpConnectionList::iterator handle_;

	unsigned long long id_;				//!< the worker-local connection-ID
	unsigned requestCount_;				//!< the number of requests already processed or currently in process

	// we could make these things below flags
	unsigned flags_;
	static const unsigned IsHandlingRequest  = 0x0002; //!< is this connection (& request) currently passed to a request handler?
	static const unsigned IsResuming         = 0x0004; //!< resume() was invoked and we've something in the pipeline (flag needed?)
	static const unsigned IsKeepAliveEnabled = 0x0008; //!< connection should keep-alive to accept further requests
	static const unsigned IsAborted          = 0x0010; //!< abort() was invoked, merely meaning, that the client aborted the connection early
	static const unsigned IsClosed           = 0x0020; //!< closed() invoked, but close-action delayed

	// HTTP HttpRequest
	Buffer input_;						//!< buffer for incoming data.
	std::size_t inputOffset_;			//!< number of bytes in input_ successfully processed already.
	std::size_t inputRequestOffset_;	//!< offset to the first byte of the currently processed request
	HttpRequest* request_;				//!< currently parsed http HttpRequest, may be NULL

	// output
	CompositeSource output_;			//!< pending write-chunks
	Socket* socket_;					//!< underlying communication socket
	SocketSink sink_;					//!< sink wrapper for socket_

	// connection abort callback
	void (*abortHandler_)(void*);
	void* abortData_;
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

inline const char* HttpConnection::status_str() const
{
	static const char* str[] = {
		"starting-up",
		"reading-request",
		"sending-reply",
		"keep-alive-read"
	};
	return str[static_cast<int>(status_)];
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
	return (flags_ & IsAborted) || isClosed();
}

inline bool HttpConnection::isClosed() const
{
	return flags_ & IsClosed;
}

/*! Tests whether or not this connection has pending data to sent to the client.
 */
inline bool HttpConnection::isOutputPending() const
{
	return !output_.empty();
}
// }}}

//@}

} // namespace x0

#endif
