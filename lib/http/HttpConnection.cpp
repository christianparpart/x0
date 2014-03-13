/* <x0/HttpConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/http/HttpConnection.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpWorker.h>
#include <x0/ServerSocket.h>
#include <x0/SocketDriver.h>
#include <x0/StackTrace.h>
#include <x0/Socket.h>
#include <x0/Types.h>
#include <x0/DebugLogger.h>
#include <x0/sysconfig.h>

#include <functional>
#include <cstdarg>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#if !defined(XZERO_NDEBUG)
#	define TRACE(level, msg...) XZERO_DEBUG("HttpConnection", (level), msg)
#else
#	define TRACE(msg...) do { } while (0)
#endif

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
 * - should request bodies land in requestBuffer_? someone with a good line could flood us w/ streaming request bodies.
 */

/** initializes a new connection object, created by given listener.
 *
 * \param lst the listener object that created this connection.
 * \note This triggers the onConnectionOpen event.
 */
HttpConnection::HttpConnection(HttpWorker* w, unsigned long long id) :
	HttpMessageParser(HttpMessageParser::REQUEST),
	refCount_(0),
	state_(Undefined),
	listener_(nullptr),
	worker_(w),
	id_(id),
	requestCount_(0),
	flags_(0),
	requestBuffer_(worker().server().maxRequestHeaderBufferSize()
                 + worker().server().maxRequestBodyBufferSize()),
	requestParserOffset_(0),
	request_(nullptr),
	output_(),
	socket_(nullptr),
	sink_(nullptr),
	autoFlush_(true),
	abortHandler_(nullptr),
	abortData_(nullptr),
	prev_(nullptr),
	next_(nullptr)
{
}

/** releases all connection resources  and triggers the onConnectionClose event.
 */
HttpConnection::~HttpConnection()
{
	TRACE(1, "%d: destructing", id_);
	if (request_)
		delete request_;

	if (socket_)
		delete socket_;
}

/**
 * Frees up any resources and resets state of this connection.
 *
 * This method is invoked only after the connection has been closed and
 * this resource may now either be physically destructed or put into
 * a list of free connection objects for later use of newly incoming
 * connections (to avoid too many memory allocations).
 */
void HttpConnection::clear()
{
	TRACE(1, "clear(): refCount: %zu, conn.state: %s, pstate: %s", refCount_, state_str(), parserStateStr());
	//TRACE(1, "Stack Trace:\n%s", StackTrace().c_str());

	HttpMessageParser::reset();

	if (request_) {
		request_->clear();
	}

	clearCustomData();

	worker_->server_.onConnectionClose(this);
	delete socket_;
	socket_ = nullptr;
	requestCount_ = 0;

	requestParserOffset_ = 0;
	requestBuffer_.clear();
}

void HttpConnection::reinitialize()
{
	flags_ = 0;
	socket_ = nullptr;
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
void HttpConnection::ref(const char* msg)
{
	++refCount_;
	TRACE(1, "ref() %u %s", refCount_, msg);
}

/** Decrements the internal reference count, marking the end of the section using this connection.
 *
 * \note After the unref()-call, the connection object MUST NOT be used any more.
 * If the unref()-call results into a reference-count of zero <b>AND</b> the connection
 * has been closed during this time, the connection will be released / destructed.
 *
 * \see ref()
 */
void HttpConnection::unref(const char* msg)
{
	--refCount_;

	TRACE(1, "unref() %u (closed:%d, outputPending:%d) %s", refCount_, isClosed(), isOutputPending(), msg);

	if (refCount_ == 0) {
		clear();
		worker_->release(this);
	}
}

void HttpConnection::io(Socket *, int revents)
{
	TRACE(1, "io(revents=%04x) %s, %s, isHandlingRequest:%d",
        revents, state_str(), parserStateStr(), isHandlingRequest());

	ref("io");

	if (revents & ev::ERROR) {
		log(Severity::error, "Potential bug in connection I/O watching. Closing.");
		abort();
		goto done;
	}

	// socket is ready for read?
	if (revents & Socket::Read) {
        if (!readSome()) {
            log(Severity::error, "readSome() failed");
            goto done;
        }
    }

	// socket is ready for write?
	if (revents & Socket::Write) {
        if (!writeSome()) {
            log(Severity::error, "writeSome() failed");
            goto done;
        }
    }

done:
	unref("io");
}

void HttpConnection::timeout(Socket *)
{
	TRACE(1, "timeout(): cstate:%s, pstate:%s", state_str(), parserStateStr());

	switch (state()) {
	case Undefined:
	case ReadingRequest:
	case ProcessingRequest:
		// we do not want further out-timing requests on this conn: just close it.
		abort(HttpStatus::RequestTimeout);
		break;
	case SendingReply:
	case SendingReplyDone:
		abort();
		break;
	case KeepAliveRead:
		close();
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

/**
 * Start first non-blocking operation for this HttpConnection.
 *
 * This is done by simply registering the underlying socket to the the I/O service
 * to watch for available input.
 *
 * @note This method must be invoked right after the object construction.
 *
 * @see close()
 * @see abort()
 * @see abort(HttpStatus)
 */
void HttpConnection::start(ServerSocket* listener, Socket* client)
{
	setState(ReadingRequest);

	listener_ = listener;

	socket_ = client;
	socket_->setReadyCallback<HttpConnection, &HttpConnection::io>(this);

	sink_.setSocket(socket_);

#if defined(TCP_NODELAY)
	if (worker_->server().tcpNoDelay())
		socket_->setTcpNoDelay(true);
#endif

	if (worker_->server().lingering())
		socket_->setLingering(worker_->server().lingering());

	TRACE(1, "starting (fd=%d)", socket_->handle());

	ref("conn-create"); // <-- this reference is being decremented in close()

	worker_->server_.onConnectionOpen(this);

	if (isAborted()) {
		// The connection got directly closed (aborted) upon connection instance creation (e.g. within the onConnectionOpen-callback),
		// so delete the object right away.
		close();
		return;
	}

	if (!request_)
		request_ = new HttpRequest(*this);

	ref("initial-read");
	if (socket_->state() == Socket::Handshake) {
		TRACE(1, "start: handshake.");
		socket_->handshake<HttpConnection, &HttpConnection::handshakeComplete>(this);
	} else {
#if defined(TCP_DEFER_ACCEPT) && defined(ENABLE_TCP_DEFER_ACCEPT)
		TRACE(1, "start: processing input");

        // TCP_DEFER_ACCEPT attempts to let us accept() only when data has arrived.
        // Thus we can go straight and attempt to read it.
        readSome();

		TRACE(1, "start: processing input done");
#else
		TRACE(1, "start: wantRead.");
		// client connected, but we do not yet know if we have data pending
		wantRead(worker_->server_.maxReadIdle());
#endif
	}
	unref("initial-read");
}

void HttpConnection::handshakeComplete(Socket *)
{
	TRACE(1, "handshakeComplete() socketState=%s", socket_->state_str());

	if (socket_->state() == Socket::Operational)
		wantRead(worker_->server_.maxReadIdle());
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
		abort(HttpStatus::BadRequest);
		return false;
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
		abort(HttpStatus::RequestHeaderFieldsTooLarge);
		return false;
	}

	// limit the number of request headers
	if (request_->requestHeaders.size() >= worker().server().maxRequestHeaderCount()) {
		abort(HttpStatus::RequestHeaderFieldsTooLarge);
		return false;
	}

	request_->requestHeaders.push_back(HttpRequestHeader(name, value));
	return true;
}

bool HttpConnection::onMessageHeaderEnd()
{
	TRACE(1, "onMessageHeaderEnd() requestParserOffset:%zu", requestParserOffset_);

	if (request_->isFinished())
		return true;

    requestHeaderEndOffset_ = requestParserOffset_;
	++requestCount_;
	flags_ |= IsHandlingRequest; // TODO: remove me, make me obsolete (replaced by state())
	setState(ProcessingRequest);

	worker_->handleRequest(request_);

	return true;
}

bool HttpConnection::onMessageContent(const BufferRef& chunk)
{
	TRACE(1, "onMessageContent(#%lu): cstate:%s pstate:%s", chunk.size(), state_str(), parserStateStr());

	request_->onRequestContent(chunk);

	return true;
}

bool HttpConnection::onMessageEnd()
{
	TRACE(1, "onMessageEnd() %s (isHandlingRequest:%d)", state_str(), isHandlingRequest());

	// marks the request-content EOS, so that the application knows when the request body
	// has been fully passed to it.
	request_->onRequestContent(BufferRef());

	// If we are currently procesing a request, then stop parsing at the end of this request.
	// The next request, if available, is being processed via resume()
	if (isHandlingRequest())
		return false;

	return true;
}

void HttpConnection::wantRead(const TimeSpan& timeout)
{
	TRACE(3, "wantRead");
	if (timeout)
		socket_->setTimeout<HttpConnection, &HttpConnection::timeout>(this, timeout.value());

	socket_->setMode(Socket::Read);
}

void HttpConnection::wantWrite()
{
	TRACE(3, "wantWrite");
	TimeSpan timeout = worker().server().maxWriteIdle();

	if (timeout)
		socket_->setTimeout<HttpConnection, &HttpConnection::timeout>(this, timeout.value());

	socket_->setMode(Socket::Write);
}

/**
 * This method gets invoked when there is data in our HttpConnection ready to read.
 *
 * We assume, that we are in request-parsing state.
 */
bool HttpConnection::readSome()
{
    TRACE(1, "readSome() state:%s", state_str());

    if (requestBuffer_.size() == requestBuffer_.capacity()) {
        TRACE(1, "readSome(): reached request buffer limit, not reading from client.");
        return true;
    }

	if (state() == KeepAliveRead) {
		TRACE(1, "readSome: state was keep-alive-read. resetting to reading-request");
		setState(ReadingRequest);
	}

    if (requestParserOffset_ < requestBuffer_.size()) {
        TRACE(1, "readSome(): we have bytes pending, so do not read now but process them right away.");
        process();
        return true;
    }

	ref("readSome");

	ssize_t rv = socket_->read(requestBuffer_, requestBuffer_.capacity());

	if (rv < 0) { // error
		switch (errno) {
		case EINTR:
		case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		case EWOULDBLOCK:
#endif
			wantRead(worker_->server_.maxReadIdle());
			break;
		default:
			log(Severity::error, "Failed to read from client. %s", strerror(errno));
			goto err;
		}
	} else if (rv == 0) {
		// EOF
		TRACE(1, "readSome: (EOF), state:%s", state_str());
		goto err;
	} else {
		TRACE(1, "readSome: read %lu bytes, state:%s", rv, state_str());
		process();
	}

	unref("readSome");
	return true;

err:
	abort();
	unref("readSome, errored.");
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

		if (autoFlush_) {
			flush();
		}
	} else {
		TRACE(1, "write() ignore chunk (%s) - (connection aborted)", chunk->className());
		delete chunk;
	}
}

void HttpConnection::flush()
{
    TRACE(1, "flush() (isOutputPending:%d)", isOutputPending());

	if (!isOutputPending())
		return;

#if defined(ENABLE_OPPORTUNISTIC_WRITE)
	writeSome();
#else
	wantWrite();
#endif
}

/**
 * Writes as much as it wouldn't block of the response stream into the underlying socket.
 *
 */
bool HttpConnection::writeSome()
{
    TRACE(1, "writeSome() state: %s", state_str());
	ref("writeSome");

	ssize_t rv = output_.sendto(sink_);

	TRACE(1, "writeSome(): sendto().rv=%lu %s", rv, rv < 0 ? strerror(errno) : "");

	if (rv > 0) {
		// output chunk written
		request_->bytesTransmitted_ += rv;
		goto done;
	}

	if (rv == 0) {
        // output fully written
		if (request_->isFinished()) {
			// finish() got invoked before reply was fully sent out, thus,
			// finalize() was delayed.
			request_->finalize();
		} else {
            // watch for EOF at least
            wantRead(TimeSpan::Zero);
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
		wantWrite();
		break;
	default:
		log(Severity::error, "Failed to write to client. %s", strerror(errno));
		goto err;
	}

done:
	unref("writeSome");
	return true;

err:
	abort();
	unref("writeSome, err'd");
	return false;
}

/*! Invokes the abort-callback (if set) and closes/releases this connection.
 *
 * \see close()
 * \see HttpRequest::finish()
 */
void HttpConnection::abort()
{
	TRACE(1, "abort() %s", isAborted() ? "is already aborted!" : "");

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
		close();
	}
}

/**
 * Aborts processing current request with given HTTP status code.
 *
 * This method is supposed to be invoked within the Request parsing state.
 * It will reply the current request with the given \p status code and close the connection.
 * This is kind of a soft abort, as the client will be notified about the soft error with a proper reply.
 *
 * \param status the HTTP status code (should be a 4xx client error).
 *
 * \see abort()
 */
void HttpConnection::abort(HttpStatus status)
{
    TRACE(1, "abort(%d): cstate:%s, pstate:%s", (int)status, state_str(), parserStateStr());

    assert(state() == ReadingRequest);

	++requestCount_;

	flags_ |= IsHandlingRequest;
	setState(ProcessingRequest);
	setShouldKeepAlive(false);

	request_->status = status;
	request_->finish();
}

/** Closes this HttpConnection, possibly deleting this object (or propagating delayed delete).
 */
void HttpConnection::close()
{
	TRACE(1, "close()");
	TRACE(2, "Stack Trace:%s\n", StackTrace().c_str());

	if (isClosed())
		// XXX should we treat this as a bug?
		return;

	flags_ |= IsClosed;

	if (state() == SendingReplyDone) {
		// usercode has issued a request->finish() but it didn't come to finalize it yet.
		request_->finalize();
	}
    setState(Undefined);

	unref("close()"); // <-- this refers to ref() in start()
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
	TRACE(1, "-- (state:%s, requestParserOffset:%lu, requestBufferSize:%lu)", state_str(), requestParserOffset_, requestBuffer_.size());

	setState(KeepAliveRead);
	request_->clear();

    // move potential pipelined-and-already-read requests to the front.
    requestBuffer_ = requestBuffer_.ref(requestParserOffset_, requestBuffer_.size() - requestParserOffset_);
    // and reset parse offset
	requestParserOffset_ = 0;

    TRACE(1, "-- moved %zu bytes pipelined request fragment data to the front", requestBuffer_.size());

    if (socket()->tcpCork())
        socket()->setTcpCork(false);

    if (requestBuffer_.empty())
        wantRead(worker().server().maxKeepAlive());
    else
        readSome();
}

/** processes a (partial) request from buffer's given \p offset of \p count bytes.
 */
bool HttpConnection::process()
{
	TRACE(2, "process: offset=%lu, size=%lu (before processing) %s, %s", requestParserOffset_, requestBuffer_.size(), state_str(), state_str());

	while (parserState() != MESSAGE_BEGIN || state() == ReadingRequest || state() == KeepAliveRead) {
		BufferRef chunk(requestBuffer_.ref(requestParserOffset_));
		if (chunk.empty())
			break;

		// ensure state is up-to-date, in case we came from keep-alive-read
		if (state() == KeepAliveRead) {
			TRACE(1, "process: status=keep-alive-read, resetting to reading-request");
			setState(ReadingRequest);
			if (request_->isFinished()) {
				TRACE(1, "process: finalizing request");
				request_->finalize();
			}
		}

		TRACE(1, "process: (size: %lu, isHandlingRequest:%d, cstate:%s pstate:%s", chunk.size(), isHandlingRequest(), state_str(), parserStateStr());
		//TRACE(1, "%s", requestBuffer_.ref(requestBuffer_.size() - rv).str().c_str());

		size_t rv = HttpMessageParser::parseFragment(chunk, &requestParserOffset_);
		TRACE(1, "process: done process()ing; fd=%d, request=%p cstate:%s pstate:%s, rv:%d", socket_->handle(), request_, state_str(), parserStateStr(), rv);

		if (isAborted()) {
			TRACE(1, "abort detected");
			return false;
		}

		if (parserState() == SYNTAX_ERROR) {
			TRACE(1, "syntax error detected");
			if (!request_->isFinished()) {
				abort(HttpStatus::BadRequest);
			}
			TRACE(1, "syntax error detected: leaving process()");
			return false;
		}

        if (!request_->isFinished()) {
            if (requestParserOffset_ >= worker().server().maxRequestHeaderBufferSize()) {
                TRACE(1, "request too large -> 413 (requestParserOffset:%zu, requestBufferSize:%zu)", requestParserOffset_, requestBuffer_.size());
                abort(HttpStatus::RequestHeaderFieldsTooLarge);
                return false;
            }
        }

		if (rv < chunk.size()) {
			request_->log(Severity::debug1, "parser aborted early.");
			return false;
		}
	}

	TRACE(1, "process: offset=%lu, bs=%lu, state=%s (after processing) io.timer:%d",
			requestParserOffset_, requestBuffer_.size(), state_str(), socket_->timerActive());

	return true;
}

unsigned int HttpConnection::remotePort() const
{
	return socket_->remotePort();
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

void HttpConnection::setState(State value)
{
#if !defined(XZERO_NDEBUG)
	static const char* str[] = {
		"undefined",
		"reading-request",
		"processing-request",
		"sending-reply",
		"sending-reply-done",
		"keep-alive-read"
	};
	(void) str;
	TRACE(1, "setState() %s => %s", str[static_cast<size_t>(state_)], str[static_cast<size_t>(value)]);
#endif

    State lastState = state_;
	state_ = value;
	worker().server().onConnectionStateChanged(this, lastState);
}


void HttpConnection::log(LogMessage&& msg)
{
	msg.addTag(!isClosed() ? remoteIP().c_str() : "(null)");

	worker().log(std::forward<LogMessage>(msg));
}

void HttpConnection::post(const std::function<void()>& function)
{
	worker_->post(function);
}

} // namespace x0
