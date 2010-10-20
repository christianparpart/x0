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
#include <x0/http/HttpServer.h>
#include <x0/http/HttpWorker.h>
#include <x0/io/CompositeSource.h>
#include <x0/io/SocketSink.h>
#include <x0/Socket.h>
#include <x0/Buffer.h>
#include <x0/Property.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <x0/sysconfig.h>

#include <functional>
#include <memory>
#include <string>
#include <ev++.h>

namespace x0 {

//! \addtogroup core
//@{

class ConnectionSink;
class HttpRequest;

class HttpConnection :
	public HttpMessageProcessor
{
public:
	HttpConnection& operator=(const HttpConnection&) = delete;
	HttpConnection(const HttpConnection&) = delete;

	/**
	 * creates an HTTP connection object.
	 * \param srv a ptr to the server object this connection belongs to.
	 */
	HttpConnection(HttpListener& listener, HttpWorker& worker, int fd);

	~HttpConnection();

	void close();

	value_property<bool> secure;				//!< true if this is a secure (HTTPS) connection, false otherwise.

	Socket *socket() const;						//!< Retrieves a pointer to the connection socket.
	HttpWorker& worker();						//!< Retrieves a reference to the owning worker.
	HttpServer& server();						//!< Retrieves a reference to the server instance.

	std::string remoteIP() const;				//!< Retrieves the IP address of the remote end point (client).
	unsigned int remotePort() const;			//!< Retrieves the TCP port numer of the remote end point (client).

	std::string localIP() const;
	unsigned int localPort() const;

	const HttpListener& listener() const;

	bool isSecure() const;

	void writeAsync(const SourcePtr& buffer, const CompletionHandlerType& handler = CompletionHandlerType());

	template<class T, class... Args> void write(Args&&... args);

private:
	friend class HttpRequest;
	friend class HttpResponse;
	friend class HttpListener;
	friend class HttpWorker;
	friend class ConnectionSink;

	// overrides from HttpMessageProcessor:
	virtual void messageBegin(BufferRef&& method, BufferRef&& entity, int version_major, int version_minor);
	virtual void messageHeader(BufferRef&& name, BufferRef&& value);
	virtual bool messageHeaderEnd();
	virtual bool messageContent(BufferRef&& chunk);
	virtual bool messageEnd();

	void start();
	void resume();

	bool isClosed() const;

	void handshakeComplete(Socket *);
	void startRead();

	void processInput();
	void processOutput();

	void process();
	void io(Socket *);
	void timeout(Socket *);

	struct ::ev_loop *loop() const;

	unsigned long long bytesTransferred() const;

public:
	std::map<HttpPlugin *, CustomDataPtr> custom_data;

private:
	HttpListener& listener_;
	HttpWorker& worker_;
	HttpServer& server_;				//!< server object owning this connection

	Socket *socket_;					//!< underlying communication socket
	bool active_;						//!< socket is active (some I/O event raised within this cycle)

	// HTTP HttpRequest
	Buffer buffer_;						//!< buffer for incoming data.
	std::size_t offset_;				//!< number of bytes in buffer_ successfully processed already.
	int request_count_;					//!< number of requests already and fully processed within this connection.
	HttpRequest *request_;				//!< currently parsed http HttpRequest, may be NULL
	HttpResponse *response_;			//!< currently processed response object, may be NULL

	CompositeSource source_;
	SocketSink sink_;
	CompletionHandlerType onWriteComplete_;
	unsigned long long bytesTransferred_;

#if !defined(NDEBUG)
	ev::tstamp ctime_;
#endif
};

// {{{ inlines
inline struct ::ev_loop* HttpConnection::loop() const
{
	return worker_.loop();
}

inline Socket *HttpConnection::socket() const
{
	return socket_;
}

inline HttpWorker& HttpConnection::worker()
{
	return worker_;
}

inline HttpServer& HttpConnection::server()
{
	return server_;
}

/** write source into the connection stream and notifies the handler on completion.
 *
 * \param buffer the buffer of bytes to be written into the connection.
 * \param handler the completion handler to invoke once the buffer has been either fully written or an error occured.
 */
inline void HttpConnection::writeAsync(const SourcePtr& buffer, const CompletionHandlerType& handler)
{
	source_.push_back(buffer);
	onWriteComplete_ = handler;

	processOutput();
}

template<class T, class... Args>
inline void HttpConnection::write(Args&&... args)
{
	source_.push_back(std::make_shared<T>(args...));

	processOutput();
}

inline const HttpListener& HttpConnection::listener() const
{
	return listener_;
}

/** tests whether HttpConnection::close() was invoked already.
 */
inline bool HttpConnection::isClosed() const
{
	return !socket_ || socket_->isClosed();
}

/** retrieves the number of transmitted bytes. */
inline unsigned long long HttpConnection::bytesTransferred() const
{
	//! \todo rename this to bytesTransmitted, and introduce bytesReceived property, so, that bytesTransferred is the sum of both.
	return bytesTransferred_;
}
// }}}

//@}

} // namespace x0

#endif
