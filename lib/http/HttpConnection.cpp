/* <x0/HttpConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
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
#	define TRACE(level, msg...) log(Severity::debug ## level, msg)
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
	HttpMessageProcessor(HttpMessageProcessor::REQUEST),
	refCount_(0),
	status_(Undefined),
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
	setStatus(ReadingRequest);
}

/** releases all connection resources  and triggers the onConnectionClose event.
 */
HttpConnection::~HttpConnection()
{
	if (request_)
		delete request_;

	clearCustomData();

	TRACE(1, "destructing (rc: %u)", refCount_);
	//TRACE(1, "Stack Trace:\n%s", StackTrace().c_str());

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
	TRACE(1, "ref() %u", refCount_);
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

	TRACE(1, "unref() %u (closed:%d, outputPending:%d)", refCount_, isClosed(), isOutputPending());

	if (refCount_ == 0) {
		worker_->release(handle_);
	}
}

void HttpConnection::io(Socket *, int revents)
{
	TRACE(1, "io(revents=%04x) isHandlingRequest:%d", revents, flags_ & IsHandlingRequest);

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

	switch (status()) {
	case ReadingRequest:
		TRACE(1, "io(): status=%s. Watch for read.", status_str());
		watchInput(worker_->server_.maxReadIdle());
		break;
	case KeepAliveRead:
		TRACE(1, "io(): status=%s. Watch for write.", status_str());
		watchInput(worker_->server_.maxKeepAlive());
		break;
	default:
		TRACE(1, "io(): status=%s. Do not touch I/O watcher.", status_str());
		break;
	}

done:
	unref();
}

void HttpConnection::timeout(Socket *)
{
	TRACE(1, "timedout: status=%s",  status_str());

	switch (status()) {
	case Undefined:
	case StartingUp:
		TRACE(1, "timeout: BUG. we should have never reached here.");
		break;
	case ReadingRequest:
		// we do not want further out-timing requests on this conn: just close it.
		setShouldKeepAlive(false);

		request_->status = HttpStatus::RequestTimeout;
		setStatus(SendingReply);
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

	TRACE(1, "starting (fd=%d)", socket_->handle());

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
		TRACE(1, "start: handshake.");
		socket_->handshake<HttpConnection, &HttpConnection::handshakeComplete>(this);
	} else {
#if defined(TCP_DEFER_ACCEPT) && defined(WITH_TCP_DEFER_ACCEPT)
		TRACE(1, "start: processing input");

		// it is ensured, that we have data pending, so directly start reading
		if (!readSome()) {
			close();
		}

		TRACE(1, "start: processing input done");
#else
		TRACE(1, "start: watchInput.");
		// client connected, but we do not yet know if we have data pending
		watchInput(worker_->server_.maxReadIdle());
#endif
	}
	unref();
}

void HttpConnection::handshakeComplete(Socket *)
{
	TRACE(1, "handshakeComplete() socketState=%s", socket_->state_str());

	if (socket_->state() == Socket::Operational)
		watchInput(worker_->server_.maxReadIdle());
	else
	{
		TRACE(1, "handshakeComplete(): handshake failed\n%s", StackTrace().c_str());
		close();
	}
}

bool HttpConnection::onMessageBegin(const BufferRef& method, const BufferRef& uri, int versionMajor, int versionMinor)
{
	TRACE(1, "onMessageBegin: '%s', '%s', HTTP/%d.%d", method.str().c_str(), uri.str().c_str(), versionMajor, versionMinor);

	request_->method = method;

	if (!request_->setUri(uri)) {
		request_->status = HttpStatus::BadRequest;
		request_->finish();
		return true;
	}

	request_->httpVersionMajor = versionMajor;
	request_->httpVersionMinor = versionMinor;

	if (request_->supportsProtocol(1, 1))
		// FIXME? HTTP/1.1 is keeping alive by default. pass "Connection: close" to close explicitely
		setShouldKeepAlive(true);
	else
		setShouldKeepAlive(false);

	// limit request uri length
	if (request_->unparsedUri.size() > worker().server().maxRequestUriSize()) {
		request_->status = HttpStatus::RequestUriTooLong;
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
		TRACE(1, "onMessageHeader() skip \"%s\": \"%s\"", name.str().c_str(), value.str().c_str());
		return true;
	}

	TRACE(1, "onMessageHeader() \"%s\": \"%s\"", name.str().c_str(), value.str().c_str());

	if (iequals(name, "Host")) {
		auto i = value.find(':');
		if (i != BufferRef::npos)
			request_->hostname = value.ref(0, i);
		else
			request_->hostname = value;
		TRACE(1, " -- hostname set to \"%s\"", request_->hostname.str().c_str());
	} else if (iequals(name, "Connection")) {
		if (iequals(value, "close"))
			setShouldKeepAlive(false);
		else if (iequals(value, "keep-alive"))
			setShouldKeepAlive(true);
	}

	// limit the size of a single request header
	if (name.size() + value.size() > worker().server().maxRequestHeaderSize()) {
		TRACE(1, "header too long. got %lu / %lu", name.size() + value.size(), worker().server().maxRequestHeaderSize());
		request_->status = HttpStatus::RequestEntityTooLarge;
		request_->finish();
		return false;
	}

	// limit the number of request headers
	if (request_->requestHeaders.size() > worker().server().maxRequestHeaderCount()) {
		TRACE(1, "header count exceeded. got %lu / %lu", request_->requestHeaders.size(), worker().server().maxRequestHeaderCount());
		request_->status = HttpStatus::RequestEntityTooLarge;
		request_->finish();
		return false;
	}

	request_->requestHeaders.push_back(HttpRequestHeader(name, value));
	return true;
}

bool HttpConnection::onMessageHeaderEnd()
{
	TRACE(1, "onMessageHeaderEnd()");

	if (request_->isFinished())
		return true;

	++requestCount_;
	flags_ |= IsHandlingRequest;
	setStatus(SendingReply);

	worker_->handleRequest(request_);

	return true;
}

bool HttpConnection::onMessageContent(const BufferRef& chunk)
{
	TRACE(1, "onMessageContent(#%lu)", chunk.size());

	request_->onRequestContent(chunk);

	return true;
}

bool HttpConnection::onMessageEnd()
{
	TRACE(1, "onMessageEnd() %s (isHandlingRequest:%d)", status_str(), flags_ & IsHandlingRequest);

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
	TRACE(1, "readSome()");

	ref();

	if (status() == KeepAliveRead) {
		TRACE(1, "readSome: status was keep-alive-read. resetting to reading-request");
		setStatus(ReadingRequest);
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
			log(Severity::error, "Failed to read from client. %s", strerror(errno));
			goto err;
		}
	} else if (rv == 0) {
		// EOF
		TRACE(1, "readSome: (EOF)");
		goto err;
	} else {
		TRACE(1, "readSome: read %lu bytes, status:%s, ros:%d", rv, status_str(), request_->outputState_);
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
		TRACE(1, "write() chunk (%s)", chunk->className());
		output_.push_back(chunk);

		watchOutput();
		//writeSome();
	} else {
		TRACE(1, "write() ignore chunk (%s) - (connection aborted)", chunk->className());
		delete chunk;
	}
}

/**
 * Writes as much as it wouldn't block of the response stream into the underlying socket.
 *
 */
bool HttpConnection::writeSome()
{
	TRACE(1, "writeSome()");
	ref();

	ssize_t rv = output_.sendto(sink_);

	TRACE(1, "writeSome(): sendto().rv=%lu %s", rv, rv < 0 ? strerror(errno) : "");

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

		TRACE(1, "writeSome: output fully written. closed:%d, outputPending:%lu, refCount:%d", isClosed(), output_.size(), refCount_);
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
		log(Severity::error, "Failed to write to client. %s", strerror(errno));
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
	TRACE(1, "abort()");

	if (isAborted())
		return;

	//assert(!isAborted() && "The connection may be only aborted once.");

	flags_ |= IsAborted;

	if (isOutputPending()) {
		TRACE(1, "abort: clearing pending output (%lu)", output_.size());
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
	TRACE(1, "close()");
	//TRACE(1, "Stack Trace:%s\n", StackTrace().c_str());

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
	TRACE(1, "resume() shouldKeepAlive:%d)", shouldKeepAlive());
	TRACE(1, "-- (status:%s, inputOffset:%lu, inputSize:%lu)", status_str(), inputOffset_, input_.size());

	setStatus(KeepAliveRead);
	request_->clear();

	if (socket()->tcpCork())
		socket()->setTcpCork(false);
}

/** processes a (partial) request from buffer's given \p offset of \p count bytes.
 */
bool HttpConnection::process()
{
	TRACE(1, "process: offset=%lu, size=%lu (before processing)", inputOffset_, input_.size());

	while (state() != MESSAGE_BEGIN || status() == ReadingRequest || status() == KeepAliveRead) {
		BufferRef chunk(input_.ref(inputOffset_));
		if (chunk.empty())
			break;

		// ensure status is up-to-date, in case we came from keep-alive-read
		if (status_ == KeepAliveRead) {
			TRACE(1, "process: status=keep-alive-read, resetting to reading-request");
			setStatus(ReadingRequest);
			if (request_->isFinished()) {
				TRACE(1, "process: finalizing request");
				request_->finalize();
			}
		}

		TRACE(1, "process: (size: %lu, isHandlingRequest:%d, state:%s status:%s", chunk.size(), (flags_ & IsHandlingRequest) != 0, state_str(), status_str());
		//TRACE(1, "%s", input_.ref(input_.size() - rv).str().c_str());

		HttpMessageProcessor::process(chunk, &inputOffset_);
		TRACE(1, "process: done process()ing; fd=%d, request=%p state:%s status:%s", socket_->handle(), request_, state_str(), status_str());

		if (isAborted()) {
			TRACE(1, "abort detected");
			return false;
		}

		if (state() == SYNTAX_ERROR) {
			TRACE(1, "syntax error detected");
			if (!request_->isFinished()) {
				flags_ |= IsHandlingRequest;
				setStatus(SendingReply);
				setShouldKeepAlive(false);
				request_->status = HttpStatus::BadRequest;
				request_->finish();
			}
			TRACE(1, "syntax error detected: leaving process()");
			return false;
		}
	}

	TRACE(1, "process: offset=%lu, bs=%lu, state=%s (after processing) io.timer:%d",
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

void HttpConnection::setShouldKeepAlive(bool enabled)
{
	TRACE(1, "setShouldKeepAlive: %d", enabled);

	if (enabled)
		flags_ |= IsKeepAliveEnabled;
	else
		flags_ &= ~IsKeepAliveEnabled;
}

void HttpConnection::setStatus(Status value)
{
	Status lastStatus = status_;
	status_ = value;
	worker().server().onConnectionStatusChanged(this, lastStatus);
}

} // namespace x0
