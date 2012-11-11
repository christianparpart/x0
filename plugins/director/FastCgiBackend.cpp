/* <plugins/director/FastCgiBackend.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

/*
 * todo:
 *     - error handling, including:
 *       - XXX early http client abort (raises EndRequestRecord-submission to application)
 *       - log stream parse errors,
 *       - transport level errors (connect/read/write errors)
 *       - timeouts
 */

#include "FastCgiBackend.h"
#include "FastCgiHealthMonitor.h"
#include "Director.h"
#include "ClassfulScheduler.h"

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <x0/io/BufferRefSource.h>
#include <x0/Logging.h>
#include <x0/strutils.h>
#include <x0/Process.h>
#include <x0/Buffer.h>
#include <x0/Types.h>
#include <x0/gai_error.h>
#include <x0/StackTrace.h>
#include <x0/sysconfig.h>

#include <system_error>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <deque>
#include <cctype>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->debug(msg))
#else
#	define TRACE(msg...) /*!*/
#endif

class FastCgiTransport : // {{{
#ifndef NDEBUG
	public x0::Logging,
#endif
	public x0::HttpMessageProcessor
{
	class ParamReader : public FastCgi::CgiParamStreamReader //{{{
	{
	private:
		FastCgiTransport *tx_;

	public:
		explicit ParamReader(FastCgiTransport *tx) : tx_(tx) {}

		virtual void onParam(const char *nameBuf, size_t nameLen, const char *valueBuf, size_t valueLen)
		{
			std::string name(nameBuf, nameLen);
			std::string value(valueBuf, valueLen);

			tx_->onParam(name, value);
		}
	}; //}}}
public:
	int refCount_;
	bool isAborted_; //!< just for debugging right now.
	FastCgiBackend *backend_;

	uint16_t id_;
	std::string backendName_;
	x0::Socket* socket_;

	x0::Buffer readBuffer_;
	size_t readOffset_;
	x0::Buffer writeBuffer_;
	size_t writeOffset_;
	bool flushPending_;

	bool configured_;

	// aka CgiRequest
	x0::HttpRequest *request_;
	FastCgi::CgiParamStreamWriter paramWriter_;

	/*! number of write chunks written within a single io() callback. */
	int writeCount_;

public:
	explicit FastCgiTransport(FastCgiBackend *cx);
	~FastCgiTransport();

	void ref();
	void unref();

	void bind(x0::HttpRequest *r, uint16_t id, x0::Socket* backend);
	void close();

	// server-to-application
	void abortRequest();

	// application-to-server
	void onStdOut(const x0::BufferRef& chunk);
	void onStdErr(const x0::BufferRef& chunk);
	void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

	FastCgiBackend& backend() const { return *backend_; }

public:
	template<typename T, typename... Args> void write(Args&&... args);
	void write(FastCgi::Type type, int requestId, x0::Buffer&& content);
	void write(FastCgi::Type type, int requestId, const char *buf, size_t len);
	void write(FastCgi::Record *record);
	void flush();

private:
	void processRequestBody(const x0::BufferRef& chunk);

	virtual bool onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool onMessageContent(const x0::BufferRef& content);

	void onWriteComplete();
	static void onClientAbort(void *p);

	void onConnectComplete(x0::Socket* s, int revents);
	void onConnectTimeout(x0::Socket* s);

	void io(x0::Socket* s, int revents);
	void onTimeout(x0::Socket* s);

	inline bool processRecord(const FastCgi::Record *record);
	void onParam(const std::string& name, const std::string& value);

	void inspect(x0::Buffer& out);
}; // }}}

// {{{ FastCgiTransport impl
FastCgiTransport::FastCgiTransport(FastCgiBackend *cx) :
	HttpMessageProcessor(x0::HttpMessageProcessor::MESSAGE),
	refCount_(1),
	isAborted_(false),
	backend_(cx),
	id_(1),
	socket_(nullptr),
	readBuffer_(),
	readOffset_(0),
	writeBuffer_(),
	writeOffset_(0),
	flushPending_(false),
	configured_(false),

	request_(nullptr),
	paramWriter_(),
	writeCount_(0)
{
#ifndef NDEBUG
	static std::atomic<int> mi(0);
	setLoggingPrefix("FastCgiTransport/%d", ++mi);
#endif
	TRACE("create");

	// stream management record: GetValues
#if 0
	FastCgi::CgiParamStreamWriter mr;
	mr.encode("FCGI_MPXS_CONNS", "");
	mr.encode("FCGI_MAX_CONNS", "");
	mr.encode("FCGI_MAX_REQS", "");
	write(FastCgi::Type::GetValues, 0, mr.output());
#endif
}

FastCgiTransport::~FastCgiTransport()
{
	TRACE("destroy");

	if (socket_) {
		if (socket_->isOpen())
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

			backend_->director()->scheduler()->schedule(request_);
		} else {
			// We actually served ths request, so finish() it.
			request_->finish();

			// Notify director that this backend has just completed a request,
			backend_->Backend::release();
		}
	}
}

void FastCgiTransport::close()
{
	if (socket_->isOpen())
		socket_->close();

	unref();
}

void FastCgiTransport::ref()
{
	++refCount_;
}

void FastCgiTransport::unref()
{
	assert(refCount_ > 0);

	--refCount_;

	if (refCount_ == 0) {
		backend_->release(this);
	}
}

void FastCgiTransport::bind(x0::HttpRequest *r, uint16_t id, x0::Socket* backend)
{
	// sanity checks
	assert(request_ == nullptr);
	assert(socket_ == nullptr);

	// initialize object
	id_ = id;
	socket_ = backend;
	backendName_ = backend->remote();
	request_ = r;
	request_->setAbortHandler(&FastCgiTransport::onClientAbort, this);

	request_->registerInspectHandler<FastCgiTransport, &FastCgiTransport::inspect>(this);

	// initialize stream
	write<FastCgi::BeginRequestRecord>(FastCgi::Role::Responder, id_, true);

	paramWriter_.encode("SERVER_SOFTWARE", PACKAGE_NAME "/" PACKAGE_VERSION);
	paramWriter_.encode("SERVER_NAME", request_->requestHeader("Host"));
	paramWriter_.encode("GATEWAY_INTERFACE", "CGI/1.1");

	paramWriter_.encode("SERVER_PROTOCOL", "1.1");
	paramWriter_.encode("SERVER_ADDR", request_->connection.localIP());
	paramWriter_.encode("SERVER_PORT", x0::lexical_cast<std::string>(request_->connection.localPort()));// TODO this should to be itoa'd only ONCE

	paramWriter_.encode("REQUEST_METHOD", request_->method);
	paramWriter_.encode("REDIRECT_STATUS", "200"); // for PHP configured with --force-redirect (Gentoo/Linux e.g.)

	request_->updatePathInfo(); // should we invoke this explicitely? I'd vote for no... however.

	paramWriter_.encode("PATH_INFO", request_->pathinfo);

	if (!request_->pathinfo.empty()) {
		paramWriter_.encode("PATH_TRANSLATED", request_->documentRoot, request_->pathinfo);
		paramWriter_.encode("SCRIPT_NAME", request_->path.ref(0, request_->path.size() - request_->pathinfo.size()));
	} else {
		paramWriter_.encode("SCRIPT_NAME", request_->path);
	}

	paramWriter_.encode("QUERY_STRING", request_->query);			// unparsed uri
	paramWriter_.encode("REQUEST_URI", request_->uri);

	//paramWriter_.encode("REMOTE_HOST", "");  // optional
	paramWriter_.encode("REMOTE_ADDR", request_->connection.remoteIP());
	paramWriter_.encode("REMOTE_PORT", x0::lexical_cast<std::string>(request_->connection.remotePort()));

	//paramWriter_.encode("AUTH_TYPE", ""); // TODO
	//paramWriter_.encode("REMOTE_USER", "");
	//paramWriter_.encode("REMOTE_IDENT", "");

	if (request_->contentAvailable()) {
		paramWriter_.encode("CONTENT_TYPE", request_->requestHeader("Content-Type"));
		paramWriter_.encode("CONTENT_LENGTH", request_->requestHeader("Content-Length"));

		request_->setBodyCallback<FastCgiTransport, &FastCgiTransport::processRequestBody>(this);
	}

#if defined(WITH_SSL)
	if (request_->connection.isSecure())
		paramWriter_.encode("HTTPS", "on");
#endif

	// HTTP request headers
	for (auto& i: request_->requestHeaders) {
		std::string key;
		key.reserve(5 + i.name.size());
		key += "HTTP_";

		for (auto p = i.name.begin(), q = i.name.end(); p != q; ++p)
			key += std::isalnum(*p) ? std::toupper(*p) : '_';

		paramWriter_.encode(key, i.value);
	}
	paramWriter_.encode("DOCUMENT_ROOT", request_->documentRoot);
	paramWriter_.encode("SCRIPT_FILENAME", request_->fileinfo->path());

	write(FastCgi::Type::Params, id_, paramWriter_.output());
	write(FastCgi::Type::Params, id_, "", 0); // EOS

	// setup I/O callback
	if (socket_->state() == x0::Socket::Connecting) {
		socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onConnectTimeout>(this, backend_->director()->connectTimeout());
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::onConnectComplete>(this);
	} else {
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::io>(this);
	}

	// flush out
	flush();
}

template<typename T, typename... Args>
inline void FastCgiTransport::write(Args&&... args)
{
	T record(args...);
	write(&record);
}

inline void FastCgiTransport::write(FastCgi::Type type, int requestId, x0::Buffer&& content)
{
	write(type, requestId, content.data(), content.size());
}

void FastCgiTransport::write(FastCgi::Type type, int requestId, const char *buf, size_t len)
{
	const size_t chunkSizeCap = 0xFFFF;
	static const char padding[8] = { 0 };

	if (len == 0) {
		FastCgi::Record record(type, requestId, 0, 0);
		TRACE("FastCgiTransport.write(type=%s, rid=%d, size=0)", record.type_str(), requestId);
		writeBuffer_.push_back(record.data(), sizeof(record));
		return;
	}

	for (size_t offset = 0; offset < len; ) {
		size_t clen = std::min(offset + chunkSizeCap, len) - offset;
		size_t plen = clen % sizeof(padding)
					? sizeof(padding) - clen % sizeof(padding)
					: 0;

		FastCgi::Record record(type, requestId, clen, plen);
		writeBuffer_.push_back(record.data(), sizeof(record));
		writeBuffer_.push_back(buf + offset, clen);
		writeBuffer_.push_back(padding, plen);

		TRACE("FastCgiTransport.write(type=%s, rid=%d, offset=%ld, size=%ld, pad=%ld)",
				record.type_str(), requestId, offset, clen, plen);

		offset += clen;
	}
}

void FastCgiTransport::write(FastCgi::Record *record)
{
	TRACE("FastCgiTransport.write(type=%s, rid=%d, size=%d, pad=%d)",
			record->type_str(), record->requestId(), record->size(), record->paddingLength());

	writeBuffer_.push_back(record->data(), record->size());
}

void FastCgiTransport::flush()
{
	if (socket_->state() == x0::Socket::Operational) {
		TRACE("flush()");
		socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onTimeout>(this, backend_->director()->writeTimeout());
		socket_->setMode(x0::Socket::ReadWrite);
	} else {
		TRACE("flush() -> pending");
		flushPending_ = true;
	}
}

void FastCgiTransport::onConnectTimeout(x0::Socket* s)
{
	request_->log(Severity::error, "http-proxy: Failed to connect to backend %s. Timed out.", backend_->name().c_str());

	if (!request_->status)
		request_->status = HttpStatus::GatewayTimedout;

	backend_->setState(HealthMonitor::State::Offline);
	close();
}

/**
 * Invoked (by open() or asynchronousely by io()) to complete the connection establishment.
 */
void FastCgiTransport::onConnectComplete(x0::Socket* s, int revents)
{
	if (s->isClosed()) {
		TRACE("onConnectComplete() connect() failed");
		request_->status = x0::HttpStatus::ServiceUnavailable;
		close();
	} else if (writeBuffer_.size() > writeOffset_ && flushPending_) {
		TRACE("onConnectComplete() flush pending");
		flushPending_ = false;
		socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onTimeout>(this, backend_->director()->writeTimeout());
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::io>(this);
		socket_->setMode(x0::Socket::ReadWrite);
	} else {
		TRACE("onConnectComplete()");
		// do not install a timeout handler here, even though, we're watching for ev::READ, because all we're to
		// get is an EOF detection (remote end-point will not sent data unless we did).
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::io>(this);
		socket_->setMode(x0::Socket::Read);
	}
}

void FastCgiTransport::io(x0::Socket* s, int revents)
{
	TRACE("io(0x%04x)", revents);

	if (revents & ev::ERROR) {
		request_->log(x0::Severity::error,
			"fastcgi: internal error occured while waiting for I/O readiness from backend application.");
		close();
		return;
	}

	ref();

	if (revents & x0::Socket::Read) {
		TRACE("io: reading ...");
		// read as much as possible
		for (;;) {
			size_t remaining = readBuffer_.capacity() - readBuffer_.size();
			if (remaining < 1024) {
				readBuffer_.reserve(readBuffer_.capacity() + 4 * 4096);
				remaining = readBuffer_.capacity() - readBuffer_.size();
			}

			int rv = socket_->read(readBuffer_);

			if (rv == 0) {
				TRACE("fastcgi: connection to backend lost.");
				goto app_err;
			}

			if (rv < 0) {
				if (errno != EINTR && errno != EAGAIN) { // TODO handle EWOULDBLOCK
					request_->log(x0::Severity::error,
						"fastcgi: read from backend %s failed: %s",
						backendName_.c_str(), strerror(errno));
					goto app_err;
				}

				break;
			}
		}

		// process fully received records
		TRACE("FastCgiTransport::io(): processing ...");
		while (readOffset_ + sizeof(FastCgi::Record) <= readBuffer_.size()) {
			const FastCgi::Record *record =
				reinterpret_cast<const FastCgi::Record *>(readBuffer_.data() + readOffset_);

			// payload fully available?
			if (readBuffer_.size() - readOffset_ < record->size())
				break;

			readOffset_ += record->size();

			if (!processRecord(record))
				goto done;
		}
	}

	if (revents & x0::Socket::Write) {
		TRACE("io(): writing to backend ...");
		ssize_t rv = socket_->write(writeBuffer_.ref(writeOffset_));
		TRACE("io(): write returned -> %ld ...", rv);

		if (rv < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				request_->log(x0::Severity::error,
					"fastcgi: write to backend %s failed. %s", backendName_.c_str(), strerror(errno));
				goto app_err;
			}

			goto done;
		}

		writeOffset_ += rv;

		// if set watcher back to EV_READ if the write-buffer has been fully written (to catch connection close events)
		if (writeOffset_ == writeBuffer_.size()) {
			TRACE("io: write buffer fully written to socket (%ld)", writeOffset_);
			socket_->setMode(x0::Socket::Read);
			writeBuffer_.clear();
			writeOffset_ = 0;
		}
	}
	goto done;

app_err:
	close();

done:
	// if we have written something to the client withing this callback and there
	// are still data chunks pending, then we must be called back on its completion,
	// so we can continue receiving more data from the backend fcgi node.
	if (writeCount_) {
		writeCount_ = 0;
		socket_->setMode(x0::Socket::None);
		ref(); // will be unref'd in completion-handler, onWriteComplete().
		request_->writeCallback<FastCgiTransport, &FastCgiTransport::onWriteComplete>(this);
	}

	unref();
}

void FastCgiTransport::onTimeout(x0::Socket* s)
{
	request_->log(Severity::error, "fastcgi: I/O timeout to backend %s. %s", backendName_.c_str(), strerror(errno));

	if (!request_->status)
		request_->status = HttpStatus::GatewayTimedout;

	backend_->setState(HealthMonitor::State::Offline);
	close();
}

bool FastCgiTransport::processRecord(const FastCgi::Record *record)
{
	TRACE("processRecord(type=%s (%d), rid=%d, contentLength=%d, paddingLength=%d)",
		record->type_str(), record->type(), record->requestId(),
		record->contentLength(), record->paddingLength());

	bool proceedHint = true;

	switch (record->type()) {
		case FastCgi::Type::GetValuesResult:
			ParamReader(this).processParams(record->content(), record->contentLength());
			configured_ = true; // should be set *only* at EOS of GetValuesResult? we currently guess, that there'll be only *one* packet
			break;
		case FastCgi::Type::StdOut:
			onStdOut(readBuffer_.ref(record->content() - readBuffer_.data(), record->contentLength()));
			break;
		case FastCgi::Type::StdErr:
			onStdErr(readBuffer_.ref(record->content() - readBuffer_.data(), record->contentLength()));
			break;
		case FastCgi::Type::EndRequest:
			onEndRequest(
				static_cast<const FastCgi::EndRequestRecord *>(record)->appStatus(),
				static_cast<const FastCgi::EndRequestRecord *>(record)->protocolStatus()
			);
			proceedHint = false;
			break;
		case FastCgi::Type::UnknownType:
		default:
			request_->log(x0::Severity::error,
				"fastcgi: unknown transport record received from backend %s. type:%d, payload-size:%ld",
				backendName_.c_str(), record->type(), record->contentLength());
#if 1
			x0::Buffer::dump(record, sizeof(record), "fcgi packet header");
			x0::Buffer::dump(record->content(), std::min(record->contentLength() + record->paddingLength(), 512), "fcgi packet payload");
#endif
			break;
	}
	return proceedHint;
}

void FastCgiTransport::onParam(const std::string& name, const std::string& value)
{
	TRACE("onParam(%s, %s)", name.c_str(), value.c_str());
}

void FastCgiTransport::abortRequest()
{
	// TODO: install deadline-timer to actually close the connection if not done by the backend.
	isAborted_ = true;
	if (socket_->isOpen()) {
		write<FastCgi::AbortRequestRecord>(id_);
		flush();
	} else {
		close();
	}
}

void FastCgiTransport::onStdOut(const x0::BufferRef& chunk)
{
	TRACE("FastCgiTransport.onStdOut: id=%d, chunk.size=%ld state=%s", id_, chunk.size(), state_str());
	process(chunk);
}

void FastCgiTransport::onStdErr(const x0::BufferRef& chunk)
{
	TRACE("FastCgiTransport.stderr(id:%d): %s", id_, chunk.chomp().str().c_str());

	if (!request_)
		return;

	request_->log(x0::Severity::error, "fastcgi: %s", chunk.chomp().str().c_str());
}

void FastCgiTransport::onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus)
{
	TRACE("FastCgiTransport.onEndRequest(appStatus=%d, protocolStatus=%d)", appStatus, (int)protocolStatus);
	close();
}

void FastCgiTransport::processRequestBody(const x0::BufferRef& chunk)
{
	TRACE("FastCgiTransport.processRequestBody(chunkLen=%ld, (r)contentLen=%ld)", chunk.size(),
			request_->connection.contentLength());

	// if chunk.size() is 0, this also marks the fcgi stdin stream's end. so just pass it.
	write(FastCgi::Type::StdIn, id_, chunk.data(), chunk.size());

	flush();
}

bool FastCgiTransport::onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value)
{
	TRACE("onResponseHeader(name:%s, value:%s)", name.str().c_str(), value.str().c_str());

	if (x0::iequals(name, "Status")) {
		int status = value.ref(0, value.find(' ')).toInt();
		request_->status = static_cast<x0::HttpStatus>(status);
	} else {
		if (name == "Location")
			request_->status = x0::HttpStatus::MovedTemporarily;

		request_->responseHeaders.push_back(name.str(), value.str());
	}

	return true;
}

bool FastCgiTransport::onMessageContent(const x0::BufferRef& content)
{
	TRACE("FastCgiTransport.messageContent(len:%ld)", content.size());

	request_->write<x0::BufferRefSource>(content);

	// if the above write() operation did not complete and thus
	// we have data pending to be sent out to the client,
	// we need to install a completion callback once
	// all (possibly proceeding write operations) have been
	// finished within a single io()-callback run.

	if (request_->connection.isOutputPending())
		++writeCount_;

	return false;
}

/**
 * write-completion hook, invoked when a content chunk is written to the HTTP client.
 */
void FastCgiTransport::onWriteComplete()
{
#if 0//{{{
	TRACE("FastCgiTransport.onWriteComplete() bufferSize: %ld", writeBuffer_.size());

	if (writeBuffer_.size() != 0) {
		TRACE("onWriteComplete: queued:%ld", writeBuffer_.size());

		request_->write<x0::BufferSource>(std::move(writeBuffer_));

		if (request_->connection.isOutputPending()) {
			TRACE("onWriteComplete: output pending. enqueue callback");
			request_->writeCallback<FastCgiTransport, &FastCgiTransport::onWriteComplete>(this);
			return;
		}
	}
#endif//}}}
	TRACE("onWriteComplete: output flushed. resume watching on app I/O (read)");

	// read next chunk from backend again
	socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onTimeout>(this, backend_->director()->readTimeout());
	socket_->setMode(x0::Socket::Read);

	// unref the ref(), invoked near the installer code of this callback
	unref();
}

/**
 * Invoked when remote client connected before the response has been fully transmitted.
 */
void FastCgiTransport::onClientAbort(void *p)
{
	FastCgiTransport* self = reinterpret_cast<FastCgiTransport*>(p);

	// notify fcgi app about client abort
	self->abortRequest();
}

void FastCgiTransport::inspect(x0::Buffer& out)
{
	//out << "Hello, World<br/>";
	out << "fcgi.refcount:" << refCount_ << ", ";
	out << "aborted:" << isAborted_ << ", ";
	socket_->inspect(out);
}
// }}}

// {{{ FastCgiBackend impl
std::atomic<uint16_t> FastCgiBackend::nextID_(0);

FastCgiBackend::FastCgiBackend(Director* director, const std::string& name, const SocketSpec& socketSpec, size_t capacity) :
	Backend(director, name, socketSpec, capacity, new FastCgiHealthMonitor(*director->worker().server().nextWorker()))
{
#ifndef NDEBUG
	setLoggingPrefix("FastCgiBackend/%s", name.c_str());
#endif

	healthMonitor().setBackend(this);
}

FastCgiBackend::~FastCgiBackend()
{
}

void FastCgiBackend::setup(const x0::SocketSpec& spec)
{
#ifndef NDEBUG
	setLoggingPrefix("FastCgiBackend/%s", spec.str().c_str());
#endif
	socketSpec_ = spec;
}

const std::string& FastCgiBackend::protocol() const
{
	static const std::string value("fastcgi");
	return value;
}

bool FastCgiBackend::process(x0::HttpRequest* r)
{
	TRACE("process()");

	x0::Socket* socket = new x0::Socket(r->connection.worker().loop());
	socket->open(socketSpec_, O_NONBLOCK | O_CLOEXEC);

	if (!socket->isOpen()) {
		r->log(x0::Severity::error, "fastcgi: connection to backend %s failed. %s", socketSpec_.str().c_str(), strerror(errno));
		delete socket;
		return false;
	}

	FastCgiTransport* transport = new FastCgiTransport(this);

	if (++nextID_ == 0)
		++nextID_;

	transport->bind(r, nextID_, socket);

	return true;
}

/**
 * \brief enqueues this transport connection ready for serving the next request.
 * \param transport the transport connection object
 */
void FastCgiBackend::release(FastCgiTransport *transport)
{
	TRACE("FastCgiBackend.release()");
	delete transport;
}
//}}}
