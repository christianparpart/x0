/* <plugins/director/HttpBackend.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "HttpBackend.h"
#include "HttpHealthMonitor.h"
#include "Director.h"
#include "ClassfulScheduler.h"

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <x0/io/BufferRefSource.h>
#include <x0/SocketSpec.h>
#include <x0/strutils.h>
#include <x0/Url.h>
#include <x0/Types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>


#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

// {{{ HttpBackend::ProxyConnection API
class HttpBackend::ProxyConnection :
#ifndef NDEBUG
	public Logging,
#endif
	public HttpMessageProcessor
{
private:
	HttpBackend* backend_;			//!< owning proxy

	int refCount_;

	HttpRequest* request_;		//!< client's request
	Socket* socket_;			//!< connection to backend app

	int connectTimeout_;
	int readTimeout_;
	int writeTimeout_;

	Buffer writeBuffer_;
	size_t writeOffset_;
	size_t writeProgress_;

	Buffer readBuffer_;
	bool processingDone_;

private:
	HttpBackend* proxy() const { return backend_; }

	void ref();
	void unref();

	inline void close();
	inline void readSome();
	inline void writeSome();

	void onConnected(Socket* s, int revents);
	void io(Socket* s, int revents);
	void onRequestChunk(const BufferRef& chunk);

	static void onAbort(void *p);
	void onWriteComplete();

	void onConnectTimeout(x0::Socket* s);
	void onTimeout(x0::Socket* s);

	// response (HttpMessageProcessor)
	virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const BufferRef& text);
	virtual bool onMessageHeader(const BufferRef& name, const BufferRef& value);
	virtual bool onMessageContent(const BufferRef& chunk);
	virtual bool onMessageEnd();

	virtual void log(x0::LogMessage&& msg);

public:
	inline explicit ProxyConnection(HttpBackend* proxy);
	~ProxyConnection();

	void start(HttpRequest* in, Socket* backend);
};
// }}}

// {{{ HttpBackend::ProxyConnection impl
HttpBackend::ProxyConnection::ProxyConnection(HttpBackend* proxy) :
	HttpMessageProcessor(HttpMessageProcessor::RESPONSE),
	backend_(proxy),
	refCount_(1),
	request_(nullptr),
	socket_(nullptr),

	connectTimeout_(0),
	readTimeout_(0),
	writeTimeout_(0),
	writeBuffer_(),
	writeOffset_(0),
	writeProgress_(0),
	readBuffer_(),
	processingDone_(false)
{
#ifndef NDEBUG
	setLoggingPrefix("ProxyConnection/%p", this);
#endif
	TRACE("ProxyConnection()");
}

HttpBackend::ProxyConnection::~ProxyConnection()
{
	TRACE("~ProxyConnection()");

	if (socket_) {
		socket_->close();

		delete socket_;
	}

	if (request_) {
		if (request_->status == HttpStatus::Undefined && !request_->isAborted()) {
			// We failed processing this request, so reschedule
			// this request within the director and give it the chance
			// to be processed by another backend,
			// or give up when the director's request processing
			// timeout has been reached.

			request_->log(Severity::notice, "Reading response from backend %s failed. Backend closed connection early.", backend_->socketSpec().str().c_str());
			backend_->reject(request_);
		} else {
			// We actually served ths request, so finish() it.
			request_->finish();

			// Notify director that this backend has just completed a request,
			backend_->release();
		}
	}
}

void HttpBackend::ProxyConnection::ref()
{
	++refCount_;
}

void HttpBackend::ProxyConnection::unref()
{
	assert(refCount_ > 0);

	--refCount_;

	if (refCount_ == 0) {
		delete this;
	}
}

void HttpBackend::ProxyConnection::close()
{
	if (socket_)
		// stop watching on any backend I/O events, if active
		socket_->close();

	unref(); // the one from the constructor
}

void HttpBackend::ProxyConnection::onAbort(void *p)
{
	ProxyConnection *self = reinterpret_cast<ProxyConnection *>(p);
	self->close();
}

void HttpBackend::ProxyConnection::start(HttpRequest* in, Socket* socket)
{
	TRACE("ProxyConnection.start(in, backend)");

	request_ = in;
	request_->setAbortHandler(&ProxyConnection::onAbort, this);
	socket_ = socket;

	// request line
	writeBuffer_.push_back(request_->method);
	writeBuffer_.push_back(' ');
	writeBuffer_.push_back(request_->unparsedUri);
	writeBuffer_.push_back(" HTTP/1.1\r\n");

	BufferRef forwardedFor;

	// request headers
	for (auto& header: request_->requestHeaders) {
		if (iequals(header.name, "X-Forwarded-For")) {
			forwardedFor = header.value;
			continue;
		}
		else if (iequals(header.name, "Content-Transfer")
				|| iequals(header.name, "Expect")
				|| iequals(header.name, "Connection")) {
			TRACE("skip requestHeader(%s: %s)", header.name.str().c_str(), header.value.str().c_str());
			continue;
		}

		TRACE("pass requestHeader(%s: %s)", header.name.str().c_str(), header.value.str().c_str());
		writeBuffer_.push_back(header.name);
		writeBuffer_.push_back(": ");
		writeBuffer_.push_back(header.value);
		writeBuffer_.push_back("\r\n");
	}

	// additional headers to add
	writeBuffer_.push_back("Connection: closed\r\n");

	// X-Forwarded-For
	writeBuffer_.push_back("X-Forwarded-For: ");
	if (forwardedFor) {
		writeBuffer_.push_back(forwardedFor);
		writeBuffer_.push_back(", ");
	}
	writeBuffer_.push_back(request_->connection.remoteIP());
	writeBuffer_.push_back("\r\n");

#if defined(WITH_SSL)
	// X-Forwarded-Proto
	if (request_->requestHeader("X-Forwarded-Proto").empty()) {
		if (request_->connection.isSecure())
			writeBuffer_.push_back("X-Forwarded-Proto: https\r\n");
		else
			writeBuffer_.push_back("X-Forwarded-Proto: http\r\n");
	}
#endif

	// request headers terminator
	writeBuffer_.push_back("\r\n");

	if (request_->contentAvailable()) {
		TRACE("start: request content available: reading.");
		request_->setBodyCallback<ProxyConnection, &ProxyConnection::onRequestChunk>(this);
	}

	if (socket_->state() == Socket::Connecting) {
		TRACE("start: connect in progress");
		socket_->setTimeout<ProxyConnection, &HttpBackend::ProxyConnection::onConnectTimeout>(this, backend_->manager()->connectTimeout());
		socket_->setReadyCallback<ProxyConnection, &ProxyConnection::onConnected>(this);
	} else { // connected
		TRACE("start: flushing");
		socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->writeTimeout());
		socket_->setReadyCallback<ProxyConnection, &ProxyConnection::io>(this);
		socket_->setMode(Socket::ReadWrite);
	}
}

/**
 * connect() timeout callback.
 *
 * This callback is invoked from within the requests associated thread to notify about
 * a timed out read/write operation.
 */
void HttpBackend::ProxyConnection::onConnectTimeout(x0::Socket* s)
{
	request_->log(Severity::error, "http-proxy: Failed to connect to backend %s. Timed out.", backend_->name().c_str());

	if (!request_->status)
		request_->status = HttpStatus::GatewayTimedout;

	backend_->setState(HealthState::Offline);
	close();
}

/**
 * read()/write() timeout callback.
 *
 * This callback is invoked from within the requests associated thread to notify about
 * a timed out read/write operation.
 */
void HttpBackend::ProxyConnection::onTimeout(x0::Socket* s)
{
	request_->log(x0::Severity::error, "http-proxy: Failed to perform I/O on backend %s. Timed out", backend_->name().c_str());
	backend_->setState(HealthState::Offline);

	if (!request_->status)
		request_->status = HttpStatus::GatewayTimedout;

	close();
}

void HttpBackend::ProxyConnection::onConnected(Socket* s, int revents)
{
	TRACE("onConnected: content? %d", request_->contentAvailable());
	//TRACE("onConnected.pending:\n%s\n", writeBuffer_.c_str());

	if (socket_->state() == Socket::Operational) {
		TRACE("onConnected: flushing");
		request_->responseHeaders.push_back("X-Director-Backend", backend_->name());
		socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->writeTimeout());
		socket_->setReadyCallback<ProxyConnection, &ProxyConnection::io>(this);
		socket_->setMode(Socket::ReadWrite); // flush already serialized request
	} else {
		TRACE("onConnected: failed");
		request_->log(Severity::error, "HTTP proxy: Could not connect to backend: %s", strerror(errno));
		backend_->setState(HealthState::Offline);
		close();
	}
}

/** transferres a request body chunk to the origin server.  */
void HttpBackend::ProxyConnection::onRequestChunk(const BufferRef& chunk)
{
	TRACE("onRequestChunk(nb:%ld)", chunk.size());
	writeBuffer_.push_back(chunk);

	if (socket_->state() == Socket::Operational) {
		socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->writeTimeout());
		socket_->setMode(Socket::ReadWrite);
	}
}

/** callback, invoked when the origin server has passed us the response status line.
 *
 * We will use the status code only.
 * However, we could pass the text field, too - once x0 core supports it.
 */
bool HttpBackend::ProxyConnection::onMessageBegin(int major, int minor, int code, const BufferRef& text)
{
	TRACE("ProxyConnection(%p).status(HTTP/%d.%d, %d, '%s')", (void*)this, major, minor, code, text.str().c_str());

	request_->status = static_cast<HttpStatus>(code);
	TRACE("status: %d", (int)request_->status);
	return true;
}

/** callback, invoked on every successfully parsed response header.
 *
 * We will pass this header directly to the client's response,
 * if that is NOT a connection-level header.
 */
bool HttpBackend::ProxyConnection::onMessageHeader(const BufferRef& name, const BufferRef& value)
{
	TRACE("ProxyConnection(%p).onHeader('%s', '%s')", (void*)this, name.str().c_str(), value.str().c_str());

	// XXX do not allow origin's connection-level response headers to be passed to the client.
	if (iequals(name, "Connection"))
		goto skip;

	if (iequals(name, "Transfer-Encoding"))
		goto skip;

	request_->responseHeaders.push_back(name.str(), value.str());
	return true;

skip:
	TRACE("skip (connection-)level header");

	return true;
}

/** callback, invoked on a new response content chunk. */
bool HttpBackend::ProxyConnection::onMessageContent(const BufferRef& chunk)
{
	TRACE("messageContent(nb:%lu) state:%s", chunk.size(), socket_->state_str());

	// stop watching for more input
	socket_->setMode(Socket::None);

	// transfer response-body chunk to client
	request_->write<BufferRefSource>(chunk);

	// start listening on backend I/O when chunk has been fully transmitted
	ref();
	request_->writeCallback<ProxyConnection, &ProxyConnection::onWriteComplete>(this);

	return true;
}

void HttpBackend::ProxyConnection::onWriteComplete()
{
	TRACE("chunk write complete: %s", state_str());
	socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->readTimeout());
	socket_->setMode(Socket::Read);
	unref();
}

bool HttpBackend::ProxyConnection::onMessageEnd()
{
	TRACE("messageEnd() backend-state:%s", socket_->state_str());
	processingDone_ = true;
	return false;
}

void HttpBackend::ProxyConnection::log(x0::LogMessage&& msg)
{
	if (request_) {
		msg.addTag("http-backend");
		request_->log(std::move(msg));
	}
}

void HttpBackend::ProxyConnection::io(Socket* s, int revents)
{
	TRACE("io(0x%04x)", revents);

	if (revents & Socket::Read)
		readSome();

	if (revents & Socket::Write)
		writeSome();
}

void HttpBackend::ProxyConnection::writeSome()
{
	TRACE("writeSome() - %s (%d)", state_str(), request_->contentAvailable());

	ssize_t rv = socket_->write(writeBuffer_.data() + writeOffset_, writeBuffer_.size() - writeOffset_);

	if (rv > 0) {
		TRACE("write request: %ld (of %ld) bytes", rv, writeBuffer_.size() - writeOffset_);

		writeOffset_ += rv;
		writeProgress_ += rv;

		if (writeOffset_ == writeBuffer_.size()) {
			TRACE("writeOffset == writeBuffser.size (%ld) p:%ld, ca: %d, clr:%ld", writeOffset_,
				writeProgress_, request_->contentAvailable(), request_->connection.contentLength());

			writeOffset_ = 0;
			writeBuffer_.clear();
			socket_->setMode(Socket::Read);
		} else {
			socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->writeTimeout());
		}
	} else if (rv < 0) {
		switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		case EWOULDBLOCK:
#endif
		case EAGAIN:
		case EINTR:
			socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->writeTimeout());
			socket_->setMode(Socket::ReadWrite);
			break;
		default:
			request_->log(Severity::error, "Writing to backend %s failed. %s", backend_->socketSpec().str().c_str(), strerror(errno));
			backend_->setState(HealthState::Offline);
			close();
			break;
		}
	}
}

void HttpBackend::ProxyConnection::readSome()
{
	TRACE("readSome() - %s", state_str());

	std::size_t lower_bound = readBuffer_.size();

	if (lower_bound == readBuffer_.capacity())
		readBuffer_.setCapacity(lower_bound + 4096);

	ssize_t rv = socket_->read(readBuffer_);

	if (rv > 0) {
		TRACE("read response: %ld bytes", rv);
		std::size_t np = process(readBuffer_.ref(lower_bound, rv));
		(void) np;
		TRACE("readSome(): process(): %ld / %ld", np, rv);

		if (processingDone_) {
			close();
		} else if (state() == SYNTAX_ERROR) {
			request_->log(Severity::error, "Reading response from backend %s failed. Syntax Error.", backend_->socketSpec().str().c_str());
			backend_->setState(HealthState::Offline);
			close();
		} else {
			TRACE("resume with io:%d, state:%s", socket_->mode(), socket_->state_str());
			socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->readTimeout());
			socket_->setMode(Socket::Read);
		}
	} else if (rv == 0) {
		TRACE("http server connection closed");
		close();
	} else {
		switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		case EWOULDBLOCK:
#endif
		case EAGAIN:
		case EINTR:
			socket_->setTimeout<ProxyConnection, &ProxyConnection::onTimeout>(this, backend_->manager()->readTimeout());
			socket_->setMode(Socket::Read);
			break;
		default:
			request_->log(Severity::error, "Reading response from backend %s failed. Syntax Error.", backend_->socketSpec().str().c_str());
			backend_->setState(HealthState::Offline);
			close();
			break;
		}
	}
}
// }}}

// {{{ HttpBackend impl
HttpBackend::HttpBackend(BackendManager* bm, const std::string& name,
		const SocketSpec& socketSpec, size_t capacity, bool healthChecks) :
	Backend(bm, name, socketSpec, capacity, healthChecks ? new HttpHealthMonitor(*bm->worker()->server().nextWorker()) : nullptr)
{
#ifndef NDEBUG
	setLoggingPrefix("HttpBackend/%s", name.c_str());
#endif

	if (healthChecks) {
		healthMonitor()->setBackend(this);
	}
}

HttpBackend::~HttpBackend()
{
}

const std::string& HttpBackend::protocol() const
{
	static const std::string value("http");
	return value;
}

bool HttpBackend::process(HttpRequest* r)
{
	TRACE("process...");

	if (Socket* socket = Socket::open(r->connection.worker().loop(), socketSpec_, O_NONBLOCK | O_CLOEXEC)) {
		TRACE("in.content? %d", r->contentAvailable());

		if (ProxyConnection* pc = new ProxyConnection(this)) {
			pc->start(r, socket);
			return true;
		}
	}

	r->log(Severity::error, "HTTP proxy: Could not connect to backend %s. %s", socketSpec_.str().c_str(), strerror(errno));
	return false;
}
// }}}
