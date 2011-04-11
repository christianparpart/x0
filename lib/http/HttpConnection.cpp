/* <x0/HttpConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpConnection.h>
#include <x0/http/HttpListener.h>
#include <x0/http/HttpRequest.h>
#include <x0/SocketDriver.h>
#include <x0/StackTrace.h>
#include <x0/Socket.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <functional>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#if !defined(NDEBUG)
#	define TRACE(msg...) this->debug(msg)
#else
#	define TRACE(msg...) do { } while (0)
#endif

#define X0_HTTP_STRICT 1

namespace x0 {

/**
 * \class HttpConnection
 * \brief represents an HTTP connection handling incoming requests.
 *
 * The \p HttpConnection object is to be allocated once an HTTP client connects
 * to the HTTP server and was accepted by the \p HttpListener.
 * It will also own the respective request and response objects created to serve
 * the requests passed through this connection.
 */

/** initializes a new connection object, created by given listener.
 * \param lst the listener object that created this connection.
 * \note This triggers the onConnectionOpen event.
 */
HttpConnection::HttpConnection(HttpListener& lst, HttpWorker& w, int fd) :
	HttpMessageProcessor(HttpMessageProcessor::REQUEST),
	secure(false),
	listener_(lst),
	worker_(w),
	socket_(0),
	buffer_(8192),
	offset_(0),
	request_count_(0),
	request_(0),
	abortHandler_(nullptr),
	abortData_(nullptr),
	source_(),
	sink_(nullptr)
#if !defined(NDEBUG)
	, ctime_(ev_now(loop()))
#endif
{
	socket_ = listener_.socketDriver()->create(loop(), fd, lst.addressFamily());
	sink_.setSocket(socket_);

#if !defined(NDEBUG)
	debug(false);
	static std::atomic<unsigned long long> id(0);
	setLoggingPrefix("Connection[%d,%s:%d]", ++id, remoteIP().c_str(), remotePort());
#endif

	TRACE("fd=%d", socket_->handle());

#if defined(TCP_NODELAY)
	if (worker_.server_.tcpNoDelay())
		socket_->setTcpNoDelay(true);
#endif

	worker_.server_.onConnectionOpen(this);
}

/** releases all connection resources  and triggers the onConnectionClose event.
 */
HttpConnection::~HttpConnection()
{
	if (request_) {
		delete request_;
#if !defined(NDEBUG)
		request_ = nullptr;
#endif
	}

	clearCustomData();

	TRACE("destructing");
	//TRACE("Stack Trace:\n%s", StackTrace().c_str());

	worker_.release(this);

	try {
		worker_.server_.onConnectionClose(this); // we cannot pass a shared pointer here as use_count is already zero and it would just lead into an exception though
	} catch (...) {
		log(Severity::error, "Unhandled exception caught on connection-close callback. Please report this bug.");
	}

	if (socket_) {
		delete socket_;
#if !defined(NDEBUG)
		socket_ = nullptr;
#endif
	}
}

void HttpConnection::io(Socket *, int revents)
{
	TRACE("io(revents=%04x)", revents);

	if (revents & Socket::Read)
		processInput();

	if (revents & Socket::Write)
		processOutput();
}

void HttpConnection::timeout(Socket *)
{
	TRACE("timed out");

	ev_unloop(loop(), EVUNLOOP_ONE);

	// XXX that's an interesting case!
	// XXX the client did not actually abort but WE declare the connection to be released early.
	abort();
}

#if defined(WITH_SSL)
bool HttpConnection::isSecure() const
{
	return listener_.isSecure();
}
#endif

/** start first async operation for this HttpConnection.
 *
 * This is done by simply registering the underlying socket to the the I/O service
 * to watch for available input.
 *
 * \note This method must be invoked right after the object construction.
 *
 * \see stop()
 */
void HttpConnection::start()
{
	if (isAborted()) {
		// The connection got directly closed (aborted) upon connection instance creation (e.g. within the onConnectionOpen-callback),
		// so delete the object right away.
		close();
		return;
	}

	if (socket_->state() == Socket::Handshake) {
		TRACE("start: handshake.");
		socket_->handshake<HttpConnection, &HttpConnection::handshakeComplete>(this);
	} else {
#if defined(TCP_DEFER_ACCEPT)
		TRACE("start: processing input");
		// it is ensured, that we have data pending, so directly start reading
		processInput();
		TRACE("start: processing input done");

		// destroy connection in case the above caused connection-close
		// XXX this is usually done within HttpConnection::io(), but we are not.
		if (isAborted()) {
			close();
		}
#else
		TRACE("start: watchInput.");
		// client connected, but we do not yet know if we have data pending
		watchInput(worker_.server_.maxReadIdle());
#endif
	}
}

void HttpConnection::handshakeComplete(Socket *)
{
	TRACE("handshakeComplete() socketState=%s", socket_->state_str());

	if (socket_->state() == Socket::Operational)
		watchInput(worker_.server_.maxReadIdle());
	else
	{
		TRACE("handshakeComplete(): handshake failed\n%s", StackTrace().c_str());
		close();
	}
}

inline bool url_decode(BufferRef& url)
{
	std::size_t left = url.offset();
	std::size_t right = left + url.size();
	std::size_t i = left; // read pos
	std::size_t d = left; // write pos
	Buffer& value = url.buffer();

	while (i != right)
	{
		if (value[i] == '%')
		{
			if (i + 3 <= right)
			{
				int ival;
				if (hex2int(value.begin() + i + 1, value.begin() + i + 3, ival))
				{
					value[d++] = static_cast<char>(ival);
					i += 3;
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else if (value[i] == '+')
		{
			value[d++] = ' ';
			++i;
		}
		else if (d != i)
		{
			value[d++] = value[i++];
		}
		else
		{
			++d;
			++i;
		}
	}

	url = value.ref(left, d - left);
	return true;
}

void HttpConnection::messageBegin(BufferRef&& method, BufferRef&& uri, int version_major, int version_minor)
{
	TRACE("messageBegin('%s', '%s', HTTP/%d.%d)", method.str().c_str(), uri.str().c_str(), version_major, version_minor);

	request_ = new HttpRequest(*this);

	request_->method = std::move(method);

	request_->uri = std::move(uri);
	url_decode(request_->uri);

	std::size_t n = request_->uri.find("?");
	if (n != std::string::npos)
	{
		request_->path = request_->uri.ref(0, n);
		request_->query = request_->uri.ref(n + 1);
	}
	else
	{
		request_->path = request_->uri;
	}

	request_->httpVersionMajor = version_major;
	request_->httpVersionMinor = version_minor;
}

void HttpConnection::messageHeader(BufferRef&& name, BufferRef&& value)
{
	if (iequals(name, "Host"))
	{
		auto i = value.find(':');
		if (i != BufferRef::npos)
			request_->hostname = value.ref(0, i);
		else
			request_->hostname = value;
	}

	request_->requestHeaders.push_back(HttpRequestHeader(std::move(name), std::move(value)));
}

bool HttpConnection::messageHeaderEnd()
{
	TRACE("messageHeaderEnd()");

#if X0_HTTP_STRICT
	BufferRef expectHeader = request_->requestHeader("Expect");
	bool content_required = request_->method == "POST" || request_->method == "PUT";

	if (content_required && request_->connection.contentLength() == -1)
	{
		request_->status = HttpError::LengthRequired;
		request_->finish();
	}
	else if (!content_required && request_->contentAvailable())
	{
		request_->status = HttpError::BadRequest; // FIXME do we have a better status code?
		request_->finish();
	}
	else if (expectHeader)
	{
		request_->expectingContinue = equals(expectHeader, "100-continue");

		if (!request_->expectingContinue || !request_->supportsProtocol(1, 1))
		{
			request_->status = HttpError::ExpectationFailed;
			request_->finish();
		}
		else
			worker_.handleRequest(request_);
	}
	else
		worker_.handleRequest(request_);
#else
	worker_.handleRequest(request_);
#endif

	return true;
}

bool HttpConnection::messageContent(BufferRef&& chunk)
{
	TRACE("messageContent(#%ld)", chunk.size());

	if (request_)
		request_->onRequestContent(std::move(chunk));

	return true;
}

bool HttpConnection::messageEnd()
{
	TRACE("messageEnd()");

	// increment the number of fully processed requests
	++request_count_;

	// XXX is this really required? (meant to mark the request-content EOS)
	if (request_)
		request_->onRequestContent(BufferRef());

	// allow continueing processing possible further requests
	return true;
}

/** Resumes processing the <b>next</b> HTTP request message within this connection.
 *
 * This method may only be invoked when the current/old request has been fully processed,
 * and is though only be invoked from within the finish handler of \p HttpRequest.
 *
 * \see HttpRequest::finish()
 */
void HttpConnection::resume()
{
	assert(request_ != nullptr);

	if (socket()->tcpCork())
		socket()->setTcpCork(false);

	// log request/response
	worker().server().onRequestDone(request_);

	delete request_;
	request_ = nullptr;

	// reset abort-callback data
	abortHandler_ = nullptr;
	abortData_ = nullptr;

	// wait for new request message, if nothing in buffer
	if (offset_ == buffer_.size())
		watchInput(worker_.server_.maxKeepAlive());
}

void HttpConnection::watchInput(const TimeSpan& timeout)
{
	if (timeout)
		socket_->setTimeout<HttpConnection, &HttpConnection::timeout>(this, timeout.value());

	socket_->setReadyCallback<HttpConnection, &HttpConnection::io>(this);
	socket_->setMode(Socket::Read);
}

void HttpConnection::watchOutput()
{
	TimeSpan timeout = worker_.server_.maxWriteIdle();

	if (timeout)
		socket_->setTimeout<HttpConnection, &HttpConnection::timeout>(this, timeout.value());

	socket_->setReadyCallback<HttpConnection, &HttpConnection::io>(this);
	socket_->setMode(Socket::Write);
}

/**
 * This method gets invoked when there is data in our HttpConnection ready to read.
 *
 * We assume, that we are in request-parsing state.
 */
void HttpConnection::processInput()
{
	TRACE("processInput()");

	ssize_t rv = socket_->read(buffer_);

	if (rv < 0) { // error
		if (errno == EAGAIN || errno == EINTR) {
			watchInput(worker_.server_.maxReadIdle());
		} else {
			log(Severity::error, "Connection read error: %s", strerror(errno));
			abort();
		}
	} else if (rv == 0) {
		// EOF
		TRACE("processInput(): (EOF)");
		abort();
	} else {
		TRACE("processInput(): read %ld bytes", rv);
		//TRACE("%s", buffer_.ref(buffer_.size() - rv).str().c_str());

		process();

		TRACE("processInput(): done process()ing; fd=%d, request=%p", socket_->handle(), request_);
	}
}

/** write source into the connection stream and notifies the handler on completion.
 *
 * \param buffer the buffer of bytes to be written into the connection.
 * \param handler the completion handler to invoke once the buffer has been either fully written or an error occured.
 */
void HttpConnection::write(Source* chunk)
{
	if (!isAborted()) {
		TRACE("write() chunk (%s)", chunk->className());
		source_.push_back(chunk);
		processOutput();
	} else {
		TRACE("write() ignore chunk (%s) - (connection aborted)", chunk->className());
		delete chunk;
	}
}

/**
 * Writes as much as it wouldn't block of the response stream into the underlying socket.
 *
 */
void HttpConnection::processOutput()
{
	TRACE("processOutput()");

	for (;;) {
		ssize_t rv = source_.sendto(sink_);

		TRACE("processOutput(): sendto().rv=%ld %s", rv, rv < 0 ? strerror(errno) : "");

		// we may not actually have created a request object here because the request-line
		// hasn't yet been parsed *BUT* we're already about send the response (e.g. 400 Bad Request),
		// so we need to first test for its existence before we access it.

		if (rv > 0) {
			// source chunk written
			if (request_) {
				request_->bytesTransmitted_ += rv;
			}
		} else if (rv == 0) {
			// source fully written
			if (request_) {
				request_->checkFinish();
			}
			break;
		} else if (errno == EAGAIN || errno == EINTR) {
			// complete write would block, so watch write-ready-event and be called back
			watchOutput();
			break;
		} else {
			// an error occurred
			log(Severity::error, "Connection write error: %s", strerror(errno));
			abort();
			break;
		}
	}
}

/*! Invokes the abort-callback (if set) and closes/releases this connection.
 *
 * \see close()
 * \see HttpRequest::finish()
 */
void HttpConnection::abort()
{
	TRACE("abort()");

	assert(!isAborted() && "The connection may be only aborted once.");

	socket_->close();

	if (abortHandler_) {
		assert(request_ != nullptr);
		socket_->setMode(Socket::None);
		abortHandler_(abortData_);
	}
}

/** Closes this HttpConnection, possibly deleting this object (or propagating delayed delete).
 */
void HttpConnection::close()
{
	TRACE("close()");
	//TRACE("Stack Trace:%s\n", StackTrace().c_str());

	socket_->close(); // XXX might be already closed (due to read/write-errors)

	if (request_) {
		// log request/response
		worker().server().onRequestDone(request_);

		delete request_;
		request_ = nullptr;
	}

	delete this;
}

/** processes a (partial) request from buffer's given \p offset of \p count bytes.
 */
void HttpConnection::process()
{
	TRACE("process: offset=%ld, size=%ld (before processing)", offset_, buffer_.size());

	HttpMessageError ec = HttpMessageProcessor::process(
			buffer_.ref(offset_, buffer_.size() - offset_),
			offset_);

	TRACE("process: offset=%ld, bs=%ld, ec=%s, state=%s (after processing)",
			offset_, buffer_.size(), std::error_code(ec).message().c_str(), state_str());

	if (isAborted())
		return;

	if (state() == SYNTAX_ERROR) {
		if (!request_)
			// in case the syntax error occured already in the request line, no request object has been instanciated yet.
			request_ = new HttpRequest(*this);

		request_->status = HttpError::BadRequest;
		request_->finish();
		return;
	}

#if 0
	if (state() == HttpMessageProcessor::MESSAGE_BEGIN) {
		// TODO reenable buffer reset (or reuse another for content! to be more huge-body friendly)
		offset_ = 0;
		buffer_.clear();
	}
#endif

	if (ec == HttpMessageError::Partial) {
		watchInput(worker_.server_.maxReadIdle());
	}
}

std::string HttpConnection::remoteIP() const
{
	return socket_->remoteIP();
}

unsigned int HttpConnection::remotePort() const
{
	return socket_->remotePort();
}

std::string HttpConnection::localIP() const
{
	return listener_.address();
}

unsigned int HttpConnection::localPort() const
{
	return socket_->localPort();
}

void HttpConnection::log(Severity s, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	char buf[512];
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	worker().server().log(s, "connection[%s]: %s", remoteIP().c_str(), buf);
}

} // namespace x0
