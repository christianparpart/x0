/* <x0/connection.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_h
#define x0_connection_h (1)

#include <x0/http/HttpMessageProcessor.h>
#include <x0/http/HttpServer.h>
#include <x0/Socket.h>
#include <x0/io/Source.h>
#include <x0/io/SocketSink.h>
#include <x0/Buffer.h>
#include <x0/Property.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <x0/sysconfig.h>

#include <functional>
#include <memory>
#include <string>
#include <ev++.h>
#include <netinet/in.h> // sockaddr_in

namespace x0 {

//! \addtogroup core
//@{

class ConnectionSink;
class HttpRequest;

/**
 * \brief represents an HTTP connection handling incoming requests.
 */
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
	explicit HttpConnection(HttpListener& listener);

	~HttpConnection();

	void start();
	void close();

	value_property<bool> secure;				//!< true if this is a secure (HTTPS) connection, false otherwise.

	Socket *socket() const;						//!< Retrieves a pointer to the connection socket.
	HttpServer& server();						//!< Retrieves a reference to the server instance.

	std::string remote_ip() const;				//!< Retrieves the IP address of the remote end point (client).
	int remote_port() const;					//!< Retrieves the TCP port numer of the remote end point (client).

	std::string local_ip() const;
	int local_port() const;

	const HttpListener& listener() const;

	bool isSecure() const;

private:
	friend class HttpRequest;
	friend class HttpResponse;
	friend class HttpListener;
	friend class ConnectionSink;

	virtual void message_begin(BufferRef&& method, BufferRef&& entity, int version_major, int version_minor);
	virtual void message_header(BufferRef&& name, BufferRef&& value);
	virtual bool message_header_done();
	virtual bool message_content(BufferRef&& chunk);
	virtual bool message_end();

	bool isClosed() const;
	void resume(bool finish);

	void handshakeComplete(Socket *);
	void start_read();
	void resume_read();

	void processInput();
	void processOutput();

	void process();
	void check_request_body();
	void writeAsync(const SourcePtr& buffer, const CompletionHandlerType& handler);
	void io(Socket *);

#if defined(WITH_CONNECTION_TIMEOUTS)
	void timeout(Socket *);
#endif

	struct ::ev_loop *loop() const;

	unsigned long long bytesTransferred() const;

public:
	std::map<HttpPlugin *, CustomDataPtr> custom_data;

private:
	HttpListener& listener_;
	HttpServer& server_;				//!< server object owning this connection

	Socket *socket_;					//!< underlying communication socket
	sockaddr_in6 saddr_;

	mutable std::string remote_ip_;		//!< internal cache to client ip
	mutable int remote_port_;			//!< internal cache to client port

	// HTTP HttpRequest
	Buffer buffer_;						//!< buffer for incoming data.
	std::size_t next_offset_;			//!< number of bytes in buffer_ successfully processed already.
	int request_count_;					//!< number of requests already processed within this connection.
	HttpRequest *request_;				//!< currently parsed http HttpRequest, may be NULL
	HttpResponse *response_;			//!< currently processed response object, may be NULL

	SourcePtr source_;
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
	return server_.loop();
}

inline Socket *HttpConnection::socket() const
{
	return socket_;
}

inline HttpServer& HttpConnection::server()
{
	return server_;
}

/** write something into the connection stream.
 * \param buffer the buffer of bytes to be written into the connection.
 * \param handler the completion handler to invoke once the buffer has been either fully written or an error occured.
 */
inline void HttpConnection::writeAsync(const SourcePtr& buffer, const CompletionHandlerType& handler)
{
	check_request_body();

	source_ = buffer;
	onWriteComplete_ = handler;

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

inline unsigned long long HttpConnection::bytesTransferred() const
{
	return bytesTransferred_;
}
// }}}

//@}

} // namespace x0

#endif
