/* <x0/HttpConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpConnection.h>
#include <x0/http/HttpRequest.h>
#include <x0/ServerSocket.h>
#include <x0/SocketDriver.h>
#include <x0/StackTrace.h>
#include <x0/Socket.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <functional>
#include <cstdarg>

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
 * to the HTTP server and was accepted by the \p ServerSocket.
 * It will also own the respective request and response objects created to serve
 * the requests passed through this connection.
 */

/* TODO
 * - should request bodies land in input_? someone with a good line could flood us w/ streaming request bodies.
 */

/** initializes a new connection object, created by given listener.
 *
 * \param lst the listener object that created this connection.
 * \note This triggers the onConnectionOpen event.
 */
HttpConnection::HttpConnection(HttpWorker* w, unsigned long long id) :
#ifndef NDEBUG
	Logging("HttpConnection"),
#endif
	HttpMessageProcessor(HttpMessageProcessor::REQUEST),
	refCount_(0),
	status_(ReadingRequest),
	listener_(nullptr),
	worker_(w),
	handle_(),
	id_(id),
	requestCount_(0),
	flags_(0),
	input_(1 * 1024),
	inputOffset_(0),
	request_(nullptr),
	output_(),
	socket_(nullptr),
	sink_(nullptr),
	abortHandler_(nullptr),
	abortData_(nullptr)
{
}

/** releases all connection resources  and triggers the onConnectionClose event.
 */
HttpConnection::~HttpConnection()
{
	if (request_)
		delete request_;

	clearCustomData();

	TRACE("destructing (rc: %ld)", refCount_);
	//TRACE("Stack Trace:\n%s", StackTrace().c_str());

	worker_->server_.onConnectionClose(this);

	delete socket_;
}

/** Increments the internal reference count and ensures that this object remains valid until its unref().
 *
 * Surround the section using this object by a ref() and unref(), ensuring, that this
 * object won't be destroyed in between.
 *
 * \see unref()
 * \see close(), resume()
 * \see HttpRequest::finish()
 */
void HttpConnection::ref()
{
	++refCount_;
	TRACE("ref() %ld", refCount_);
}

/** Decrements the internal reference count, marking the end of the section using this connection.
 *
 * \note After the unref()-call, the connection object MUST NOT be used any more.
 * If the unref()-call results into a reference-count of zero <b>AND</b> the connection
 * has been closed during this time, the connection will be released / destructed.
 *
 * \see ref()
 */
void HttpConnection::unref()
{
	--refCount_;

	TRACE("unref() %ld (closed:%d, outputPending:%d)", refCount_, isClosed(), isOutputPending());

	if (refCount_ == 0) {
		worker_->release(handle_);
	}
}

void HttpConnection::io(Socket *, int revents)
{
	TRACE("io(revents=%04x) isHandlingRequest:%d", revents, flags_ & IsHandlingRequest);

	ref();

	if (revents & ev::ERROR) {
		log(Severity::error, "Potential bug in connection I/O watching. Closing.");
		abort();
		goto done;
	}

	if ((revents & Socket::Read) && !readSome())
		goto done;

	if ((revents & Socket::Write) && !writeSome())
		goto done;

	if (!process())
		goto done;

	switch (status()) {
	case ReadingRequest:
		watchInput(worker_->server_.maxReadIdle());
		break;
	case KeepAliveRead:
		watchInput(worker_->server_.maxKeepAlive());
	default:
		break;
	}

done:
	unref();
}

void HttpConnection::timeout(Socket *)
{
	TRACE("timedout: status=%s",  status_str());

	switch (status()) {
	case StartingUp:
		TRACE("timeout: BUG. we should have never reached here.");
		break;
	case ReadingRequest:
		// we do not want further out-timing requests on this conn: just close it.
		setShouldKeepAlive(false);

		request_->status = HttpError::RequestTimeout;
		status_ = SendingReply;
		request_->finish();
		break;
	case KeepAliveRead:
		close();
		break;
	case SendingReply:
		abort();
		break;
	}
}

bool HttpConnection::isSecure() const
{
#if defined(WITH_SSL)
	return listener_->socketDriver()->isSecure();
#else
	return false;
#endif
}

/** start first async operation for this HttpConnection.
 *
 * This is done by simply registering the underlying socket to the the I/O service
 * to watch for available input.
 *
 * \note This method must be invoked right after the object construction.
 *
 * \see stop()
 */
void HttpConnection::start(ServerSocket* listener, Socket* client, const HttpWorker::ConnectionHandle& handle)
{
	handle_ = handle;
	listener_ = listener;

	socket_ = client;
	socket_->setReadyCallback<HttpConnection, &HttpConnection::io>(this);

	sink_.setSocket(socket_);

#if defined(TCP_NODELAY)
	if (worker_->server().tcpNoDelay())
		socket_->setTcpNoDelay(true);
#endif

#if !defined(NDEBUG)
	setLoggingPrefix("HttpConnection[%d,%llu|%s:%d]", worker_->id(), id_, remoteIP().c_str(), remotePort());
#endif

	TRACE("starting (fd=%d)", socket_->handle());

	ref(); // <-- this reference is being decremented in close()

	worker_->server_.onConnectionOpen(this);

	if (isAborted()) {
		// The connection got directly closed (aborted) upon connection instance creation (e.g. within the onConnectionOpen-callback),
		// so delete the object right away.
		close();
		return;
	}

	request_ = new HttpRequest(*this);

	ref();
	if (socket_->state() == Socket::Handshake) {
		TRACE("start: handshake.");
		socket_->handshake<HttpConnection, &HttpConnection::handshakeComplete>(this);
	} else {
#if defined(TCP_DEFER_ACCEPT) && defined(WITH_TCP_DEFER_ACCEPT)
		TRACE("start: processing input");

		// it is ensured, that we have data pending, so directly start reading
		if (readSome())
			process();
		else
			close();

		TRACE("start: processing input done");
#else
		TRACE("start: watchInput.");
		// client connected, but we do not yet know if we have data pending
		watchInput(worker_->server_.maxReadIdle());
#endif
	}
	unref();
}

void HttpConnection::handshakeComplete(Socket *)
{
	TRACE("handshakeComplete() socketState=%s", socket_->state_str());

	if (socket_->state() == Socket::Operational)
		watchInput(worker_->server_.maxReadIdle());
	else
	{
		TRACE("handshakeComplete(): handshake failed\n%s", StackTrace().c_str());
		close();
	}
}

inline bool url_decode(Buffer& value, BufferRef& url)
{
	assert(url.belongsTo(value));

	std::size_t left = url.begin() - value.begin();
	std::size_t right = left + url.size();
	std::size_t i = left; // read pos
	std::size_t d = left; // write pos

	while (i != right) {
		if (value[i] == '%') {
			if (i + 3 <= right) {
				int ival;
				if (hex2int(value.begin() + i + 1, value.begin() + i + 3, ival)) {
					value[d++] = static_cast<char>(ival);
					i += 3;
				} else {
					return false;
				}
			} else {
				return false;
			}
		} else if (value[i] == '+') {
			value[d++] = ' ';
			++i;
		} else if (d != i) {
			value[d++] = value[i++];
		} else {
			++d;
			++i;
		}
	}

	url = value.ref(left, d - left);
	return true;
}

bool HttpConnection::onMessageBegin(const BufferRef& method, const BufferRef& uri, int versionMajor, int versionMinor)
{
	TRACE("onMessageBegin: '%s', '%s', HTTP/%d.%d", method.str().c_str(), uri.str().c_str(), versionMajor, versionMinor);

	request_->method = method;
	request_->uri = uri;
	url_decode(input_, request_->uri);

	std::size_t n = request_->uri.find("?");
	if (n != std::string::npos) {
		request_->path = request_->uri.ref(0, n);
		request_->query = request_->uri.ref(n + 1);
	} else {
		request_->path = request_->uri;
	}

	request_->httpVersionMajor = versionMajor;
	request_->httpVersionMinor = versionMinor;

	if (request_->supportsProtocol(1, 1))
		// FIXME? HTTP/1.1 is keeping alive by default. pass "Connection: close" to close explicitely
		setShouldKeepAlive(true);
	else
		setShouldKeepAlive(false);

	// limit request uri length
	if (request_->uri.size() > worker().server().maxRequestUriSize()) {
		request_->status = HttpError::RequestUriTooLong;
		request_->finish();
		return false;
	}

	return true;
}

bool HttpConnection::onMessageHeader(const BufferRef& name, const BufferRef& value)
{
	if (request_->isFinished()) {
		// this can happen when the request has failed some checks and thus,
		// a client error message has been sent already.
		// we need to "parse" the remaining content anyways.
		TRACE("onMessageHeader() skip \"%s\": \"%s\"", name.str().c_str(), value.str().c_str());
		return true;
	}

	TRACE("onMessageHeader() \"%s\": \"%s\"", name.str().c_str(), value.str().c_str());

	if (iequals(name, "Host")) {
		auto i = value.find(':');
		if (i != BufferRef::npos)
			request_->hostname = value.ref(0, i);
		else
			request_->hostname = value;
		TRACE(" -- hostname set to \"%s\"", request_->hostname.str().c_str());
	} else if (iequals(name, "Connection")) {
		if (iequals(value, "close"))
			setShouldKeepAlive(false);
		else if (iequals(value, "keep-alive"))
			setShouldKeepAlive(true);
	}

	// limit the size of a single request header
	if (name.size() + value.size() > worker().server().maxRequestHeaderSize()) {
		TRACE("header too long. got %ld / %ld", name.size() + value.size(), worker().server().maxRequestHeaderSize());
		request_->status = HttpError::RequestEntityTooLarge;
		request_->finish();
		return false;
	}

	// limit the number of request headers
	if (request_->requestHeaders.size() > worker().server().maxRequestHeaderCount()) {
		TRACE("header count exceeded. got %ld / %ld", request_->requestHeaders.size(), worker().server().maxRequestHeaderCount());
		request_->status = HttpError::RequestEntityTooLarge;
		request_->finish();
		return false;
	}

	request_->requestHeaders.push_back(HttpRequestHeader(name, value));
	return true;
}

bool HttpConnection::onMessageHeaderEnd()
{
	TRACE("onMessageHeaderEnd()");

	if (request_->isFinished())
		return true;

#if X0_HTTP_STRICT
	BufferRef expectHeader = request_->requestHeader("Expect");
	bool contentRequired = request_->method == "POST" || request_->method == "PUT";

	if (contentRequired) {
		if (request_->connection.contentLength() == -1) {
			request_->status = HttpError::LengthRequired;
			request_->finish();
			return true;
		}
	} else {
		if (request_->contentAvailable()) {
			request_->status = HttpError::BadRequest; // FIXME do we have a better status code?
			request_->finish();
			return true;
		}
	}

	if (expectHeader) {
		request_->expectingContinue = equals(expectHeader, "100-continue");

		if (!request_->expectingContinue || !request_->supportsProtocol(1, 1)) {
			request_->status = HttpError::ExpectationFailed;
			request_->finish();
			return true;
		}
	}
#endif

	++requestCount_;
	++worker_->requestCount_;

	flags_ |= IsHandlingRequest;
	status_ = SendingReply;

	worker_->handleRequest(request_);

	return true;
}

bool HttpConnection::onMessageContent(const BufferRef& chunk)
{
	TRACE("onMessageContent(#%ld)", chunk.size());

	request_->onRequestContent(chunk);

	return true;
}

bool HttpConnection::onMessageEnd()
{
	TRACE("onMessageEnd() %s (isHandlingRequest:%d)", status_str(), flags_ & IsHandlingRequest);

	// marks the request-content EOS, so that the application knows when the request body
	// has been fully passed to it.
	request_->onRequestContent(BufferRef());

	// If we are currently procesing a request, then stop parsing at the end of this request.
	// The next request, if available, is being processed via resume()
	if (flags_ & IsHandlingRequest)
		return false;

	return true; //shouldKeepAlive();
}

void HttpConnection::watchInput(const TimeSpan& timeout)
{
	if (timeout)
		socket_->setTimeout<HttpConnection, &HttpConnection::timeout>(this, timeout.value());

	socket_->setMode(Socket::Read);
}

void HttpConnection::watchOutput()
{
	TimeSpan timeout = worker_->server_.maxWriteIdle();

	if (timeout)
		socket_->setTimeout<HttpConnection, &HttpConnection::timeout>(this, timeout.value());

	socket_->setMode(Socket::ReadWrite);
}

/**
 * This method gets invoked when there is data in our HttpConnection ready to read.
 *
 * We assume, that we are in request-parsing state.
 */
bool HttpConnection::readSome()
{
	TRACE("readSome()");

	ref();

	if (status() == KeepAliveRead) {
		TRACE("readSome: status was keep-alive-read. resetting to reading-request", request_->outputState_);
		status_ = ReadingRequest;
	}

	ssize_t rv = socket_->read(input_);

	if (rv < 0) { // error
		switch (errno) {
		case EINTR:
		case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		case EWOULDBLOCK:
#endif
			watchInput(worker_->server_.maxReadIdle());
			break;
		default:
			log(Severity::error, "Connection read error: %s", strerror(errno));
			goto err;
		}
	} else if (rv == 0) {
		// EOF
		TRACE("readSome: (EOF)");
		goto err;
	} else {
		TRACE("readSome: read %ld bytes, status:%s, ros:%d", rv, status_str(), request_->outputState_);
		process();
	}

	unref();
	return true;

err:
	abort();
	unref();
	return false;
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
		output_.push_back(chunk);

		watchOutput();
		//writeSome();
	} else {
		TRACE("write() ignore chunk (%s) - (connection aborted)", chunk->className());
		delete chunk;
	}
}

/**
 * Writes as much as it wouldn't block of the response stream into the underlying socket.
 *
 */
bool HttpConnection::writeSome()
{
	TRACE("writeSome()");
	ref();

	ssize_t rv = output_.sendto(sink_);

	TRACE("writeSome(): sendto().rv=%ld %s", rv, rv < 0 ? strerror(errno) : "");

	if (rv > 0) {
		// output chunk written
		request_->bytesTransmitted_ += rv;
		goto done;
	}

	if (rv == 0) {
		// output fully written
		watchInput();

		if (request_->isFinished()) {
			// finish() got invoked before reply was fully sent out, thus,
			// finalize() was delayed.
			request_->finalize();
		}

		TRACE("writeSome: output fully written. closed:%d, outputPending:%ld, refCount:%d", isClosed(), output_.size(), refCount_);
		goto done;
	}

	// sendto() failed
	switch (errno) {
	case EINTR:
		break;
	case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
	case EWOULDBLOCK:
#endif
		// complete write would block, so watch write-ready-event and be called back
		watchOutput();
		break;
	default:
		log(Severity::error, "Connection write error: %s", strerror(errno));
		goto err;
	}

done:
	unref();
	return true;

err:
	abort();
	unref();
	return false;
}

/*! Invokes the abort-callback (if set) and closes/releases this connection.
 *
 * \see close()
 * \see HttpRequest::finish()
 */
void HttpConnection::abort()
{
	TRACE("abort()");

	if (isAborted())
		return;

	//assert(!isAborted() && "The connection may be only aborted once.");

	flags_ |= IsAborted;

	if (isOutputPending()) {
		TRACE("abort: clearing pending output (%ld)", output_.size());
		output_.clear();
	}

	if (abortHandler_) {
		assert(request_ != nullptr);

		// the client aborted the connection, so close the client-socket right away to safe resources
		socket_->close();

		abortHandler_(abortData_);
	} else {
		request_->clearCustomData();
		close();
	}
}

/** Closes this HttpConnection, possibly deleting this object (or propagating delayed delete).
 */
void HttpConnection::close()
{
	TRACE("close()");
	//TRACE("Stack Trace:%s\n", StackTrace().c_str());

	if (isClosed())
		// XXX should we treat this as a bug?
		return;

	flags_ |= IsClosed;

	unref(); // <-- this refers to ref() in start()
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
	TRACE("resume() shouldKeepAlive:%d)", shouldKeepAlive());
	TRACE("-- (status:%s, inputOffset:%ld, inputSize:%ld)", status_str(), inputOffset_, input_.size());

	status_ = KeepAliveRead;
	request_->clear();

	if (socket()->tcpCork())
		socket()->setTcpCork(false);
}

/** processes a (partial) request from buffer's given \p offset of \p count bytes.
 */
bool HttpConnection::process()
{
	TRACE("process: offset=%ld, size=%ld (before processing)", inputOffset_, input_.size());

	while (state() != MESSAGE_BEGIN || status() == ReadingRequest || status() == KeepAliveRead) {
		BufferRef chunk(input_.ref(inputOffset_));
		if (chunk.empty())
			break;

		// ensure status is up-to-date, in case we came from keep-alive-read
		if (status_ == KeepAliveRead) {
			TRACE("process: status=keep-alive-read, resetting to reading-request");
			status_ = ReadingRequest;
			if (request_->isFinished()) {
				TRACE("process: finalizing request");
				request_->finalize();
			}
		}

		TRACE("process: (size: %ld, isHandlingRequest:%d, state:%s status:%s", chunk.size(), (flags_ & IsHandlingRequest) != 0, state_str(), status_str());
		//TRACE("%s", input_.ref(input_.size() - rv).str().c_str());

		HttpMessageProcessor::process(chunk, &inputOffset_);
		TRACE("process: done process()ing; fd=%d, request=%p state:%s status:%s", socket_->handle(), request_, state_str(), status_str());

		if (isAborted())
			return false;

		if (state() == SYNTAX_ERROR) {
			if (!request_->isFinished()) {
				setShouldKeepAlive(false);
				request_->status = HttpError::BadRequest;
				request_->finish();
			}
			return false;
		}
	}

	TRACE("process: offset=%ld, bs=%ld, state=%s (after processing) io.timer:%d",
			inputOffset_, input_.size(), state_str(), socket_->timerActive());

	return true;
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
	return listener_->address();
}

unsigned int HttpConnection::localPort() const
{
	return listener_->port();
}

void HttpConnection::log(Severity s, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	char buf[512];
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	worker().server().log(s, "connection[%s]: %s", !isClosed() ? remoteIP().c_str() : "(null)", buf);
}


void HttpConnection::setShouldKeepAlive(bool enabled)
{
	TRACE("setShouldKeepAlive: %d", enabled);

	if (enabled)
		flags_ |= IsKeepAliveEnabled;
	else
		flags_ &= ~IsKeepAliveEnabled;
}

} // namespace x0
