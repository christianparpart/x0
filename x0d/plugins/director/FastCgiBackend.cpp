/* <plugins/director/FastCgiBackend.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
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

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <x0/io/BufferRefSource.h>
#include <x0/io/FileSource.h>
#include <x0/Logging.h>
#include <x0/strutils.h>
#include <x0/Process.h>
#include <x0/Buffer.h>
#include <x0/Types.h>
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

#if 1 //!defined(XZERO_NDEBUG)
#	define TRACE(level, msg...) { \
		static_assert((level) >= 1 && (level) <= 5, "TRACE()-level must be between 1 and 5, matching Severity::debugN values."); \
		log(Severity::debug ## level, msg); \
	}
#else
#	define TRACE(msg...) /*!*/
#endif

using x0::Severity;
using x0::LogMessage;

std::atomic<unsigned long long> transportIds_(0);

class FastCgiTransport : // {{{
#ifndef XZERO_NDEBUG
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
	unsigned long long transportId_;
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
	RequestNotes* rn_;

	/*! number of write chunks written within a single io() callback. */
	int writeCount_;

	int transferHandle_;
	int transferOffset_;

	std::string sendfile_;

public:
	explicit FastCgiTransport(FastCgiBackend* cx, RequestNotes* rn, uint16_t id, x0::Socket* backend);
	~FastCgiTransport();

	void ref();
	void unref();

	void bind(RequestNotes* rn);
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
	virtual bool onMessageHeaderEnd();
	virtual bool onMessageContent(const x0::BufferRef& content);

	virtual void log(x0::LogMessage&& msg);

	template<typename... Args>
	void log(Severity severity, const char* fmt, Args&&... args);

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
FastCgiTransport::FastCgiTransport(FastCgiBackend* cx, RequestNotes* rn, uint16_t id, x0::Socket* upstream) :
	HttpMessageProcessor(x0::HttpMessageProcessor::MESSAGE),
	transportId_(++transportIds_),
	refCount_(1),
	isAborted_(false),
	backend_(cx),
	id_(id),
	backendName_(upstream->remote()),
	socket_(upstream),
	readBuffer_(),
	readOffset_(0),
	writeBuffer_(),
	writeOffset_(0),
	flushPending_(false),
	configured_(false),

	rn_(nullptr),
	writeCount_(0),
	transferHandle_(-1),
	transferOffset_(0),
	sendfile_()
{
#ifndef XZERO_NDEBUG
	static std::atomic<int> mi(0);
	setLoggingPrefix("FastCgiTransport/%d", ++mi);
#endif
	TRACE(1, "create");

	// stream management record: GetValues
#if 0
	FastCgi::CgiParamStreamWriter mr;
	mr.encode("FCGI_MPXS_CONNS", "");   // defaults to 1
	mr.encode("FCGI_MAX_CONNS", "");    // defaults to 1
	mr.encode("FCGI_MAX_REQS", "");     // defaults to 1
	write(FastCgi::Type::GetValues, 0, mr.output());
#endif

	bind(rn);
}

FastCgiTransport::~FastCgiTransport()
{
	TRACE(1, "closing transport connection to upstream server.");

	if (socket_) {
		if (socket_->isOpen())
			socket_->close();

		delete socket_;
	}

	if (!(transferHandle_ < 0)) {
		::close(transferHandle_);
	}

	if (rn_) {
		if (rn_->request->status == HttpStatus::Undefined && !rn_->request->isAborted()) {
			// We failed processing this request, so reschedule
			// this request within the director and give it the chance
			// to be processed by another backend,
			// or give up when the director's request processing
			// timeout has been reached.

			backend_->manager()->reject(rn_);
		} else {
			// Notify director that this backend has just completed a request,
			backend_->release(rn_);

			// We actually served ths request, so finish() it.
			rn_->request->finish();
		}
	}
}

void FastCgiTransport::close()
{
	TRACE(1, "Closing transport connection.");

	if (socket_->isOpen())
		socket_->close();

	unref(); // related to the increment in contructer FastCgiTransport()
}

void FastCgiTransport::ref()
{
	++refCount_;

	TRACE(1, "Incrementing reference count to %d.", refCount_);
}

void FastCgiTransport::unref()
{
	TRACE(1, "Decrementing reference count from %d.", refCount_);

	assert(refCount_ > 0);

	--refCount_;

	if (refCount_ == 0) {
		backend_->release(this);
	}
}

/**
 * @brief binds the given request to this FastCGI transport connection.
 *
 * @param r the HTTP client request to bind to this FastCGI transport connection.
 *
 * Requests bound to a FastCGI transport will be passed to the connected 
 * transport backend and served by it.
 */
void FastCgiTransport::bind(RequestNotes* rn)
{
	rn_ = rn;
	auto r = rn_->request;

	// initialize object
	r->setAbortHandler(&FastCgiTransport::onClientAbort, this);

	r->registerInspectHandler<FastCgiTransport, &FastCgiTransport::inspect>(this);

	// initialize stream
	write<FastCgi::BeginRequestRecord>(FastCgi::Role::Responder, id_, true);

	FastCgi::CgiParamStreamWriter params;
	params.encode("SERVER_SOFTWARE", PACKAGE_NAME "/" PACKAGE_VERSION);
	params.encode("SERVER_NAME", r->requestHeader("Host"));
	params.encode("GATEWAY_INTERFACE", "CGI/1.1");

	params.encode("SERVER_PROTOCOL", "1.1");
	params.encode("SERVER_ADDR", r->connection.localIP());
	params.encode("SERVER_PORT", x0::lexical_cast<std::string>(r->connection.localPort()));// TODO this should to be itoa'd only ONCE

	params.encode("REQUEST_METHOD", r->method);
	params.encode("REDIRECT_STATUS", "200"); // for PHP configured with --force-redirect (Gentoo/Linux e.g.)

	r->updatePathInfo(); // should we invoke this explicitely? I'd vote for no... however.

	params.encode("PATH_INFO", r->pathinfo);

	if (!r->pathinfo.empty()) {
		params.encode("PATH_TRANSLATED", r->documentRoot, r->pathinfo);
		params.encode("SCRIPT_NAME", r->path.ref(0, r->path.size() - r->pathinfo.size()));
	} else {
		params.encode("SCRIPT_NAME", r->path);
	}

	params.encode("QUERY_STRING", r->query);			// unparsed uri
	params.encode("REQUEST_URI", r->unparsedUri);

	//params.encode("REMOTE_HOST", "");  // optional
	params.encode("REMOTE_ADDR", r->connection.remoteIP());
	params.encode("REMOTE_PORT", x0::lexical_cast<std::string>(r->connection.remotePort()));

	//params.encode("AUTH_TYPE", ""); // TODO
	//params.encode("REMOTE_USER", "");
	//params.encode("REMOTE_IDENT", "");

	if (r->contentAvailable()) {
		params.encode("CONTENT_TYPE", r->requestHeader("Content-Type"));
		params.encode("CONTENT_LENGTH", r->requestHeader("Content-Length"));

		r->setBodyCallback<FastCgiTransport, &FastCgiTransport::processRequestBody>(this);
	}

	if (r->connection.isSecure())
		params.encode("HTTPS", "on");

	// HTTP request headers
	for (auto& i: r->requestHeaders) {
		std::string key;
		key.reserve(5 + i.name.size());
		key += "HTTP_";

		for (auto p = i.name.begin(), q = i.name.end(); p != q; ++p)
			key += std::isalnum(*p) ? std::toupper(*p) : '_';

		params.encode(key, i.value);
	}
	params.encode("DOCUMENT_ROOT", r->documentRoot);

	if (r->fileinfo) {
		params.encode("SCRIPT_FILENAME", r->fileinfo->path());
	}

	write(FastCgi::Type::Params, id_, params.output());
	write(FastCgi::Type::Params, id_, "", 0); // EOS

	// setup I/O callback
	if (socket_->state() == x0::Socket::Connecting) {
		socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onConnectTimeout>(this, backend_->manager()->connectTimeout());
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::onConnectComplete>(this);
	} else {
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::io>(this);
	}

	// flush out
	flush();

	if (backend_->manager()->transferMode() == TransferMode::FileAccel) {
		char path[1024];
		snprintf(path, sizeof(path), "/tmp/x0d-director-%d", socket_->handle());

		transferHandle_ = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (transferHandle_ < 0) {
			r->log(Severity::error, "Could not open temporary file %s. %s", path, strerror(errno));
		}
	}
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
		TRACE(1, "writing packet (%s) of %ld bytes to upstream server.", record.type_str(), len);
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

		offset += clen;

		TRACE(1, "writing packet (%s) of %ld bytes to upstream server.", record.type_str(), record.size());
	}
}

void FastCgiTransport::write(FastCgi::Record *record)
{
	TRACE(1, "writing packet (%s) of %ld bytes to upstream server.", record->type_str(), record->size());

	writeBuffer_.push_back(record->data(), record->size());
}

void FastCgiTransport::flush()
{
	if (socket_->state() == x0::Socket::Operational) {
		TRACE(1, "flushing pending data to upstream server.");
		socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onTimeout>(this, backend_->manager()->writeTimeout());
		socket_->setMode(x0::Socket::ReadWrite);
	} else {
		TRACE(1, "mark pending data to be flushed to upstream server.");
		flushPending_ = true;
	}
}

void FastCgiTransport::onConnectTimeout(x0::Socket* s)
{
	log(x0::Severity::error, "Trying to connect to upstream server %s was timing out.", backend_->name().c_str());

	if (!rn_->request->status)
		rn_->request->status = HttpStatus::GatewayTimeout;

	backend_->setState(HealthState::Offline);
	close();
}

/**
 * Invoked (by open() or asynchronousely by io()) to complete the connection establishment.
 */
void FastCgiTransport::onConnectComplete(x0::Socket* s, int revents)
{
	if (s->isClosed()) {
		log(x0::Severity::error, "connection to upstream server failed. %s", strerror(errno));
		rn_->request->status = x0::HttpStatus::ServiceUnavailable;
		close();
	} else if (writeBuffer_.size() > writeOffset_ && flushPending_) {
		TRACE(1, "Connected. Flushing pending data.");
		flushPending_ = false;
		socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onTimeout>(this, backend_->manager()->writeTimeout());
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::io>(this);
		socket_->setMode(x0::Socket::ReadWrite);
	} else {
		TRACE(1, "Connected.");
		// do not install a timeout handler here, even though, we're watching for ev::READ, because all we're to
		// get is an EOF detection (remote end-point will not sent data unless we did).
		socket_->setReadyCallback<FastCgiTransport, &FastCgiTransport::io>(this);
		socket_->setMode(x0::Socket::Read);
	}
}

void FastCgiTransport::io(x0::Socket* s, int revents)
{
	TRACE(1, "Received I/O activity on upstream socket. revents=0x%04x", revents);

	if (revents & ev::ERROR) {
		log(x0::Severity::error, "Internal error occured while waiting for I/O readiness from backend application.");
		close();
		return;
	}

	ref();

	if (revents & x0::Socket::Read) {
		TRACE(1, "reading from upstream server.");
		// read as much as possible
		for (;;) {
			size_t remaining = readBuffer_.capacity() - readBuffer_.size();
			if (remaining < 1024) {
				readBuffer_.reserve(readBuffer_.capacity() + 4 * 4096);
				remaining = readBuffer_.capacity() - readBuffer_.size();
			}

			int rv = socket_->read(readBuffer_);

			if (rv == 0) {
				log(x0::Severity::error, "Connection to backend lost.");
				goto app_err;
			}

			if (rv < 0) {
				if (errno != EINTR && errno != EAGAIN) { // TODO handle EWOULDBLOCK
					log(x0::Severity::error, "Read from backend %s failed: %s", backendName_.c_str(), strerror(errno));
					goto app_err;
				}

				break;
			}
		}

		// process fully received records
		while (readOffset_ + sizeof(FastCgi::Record) <= readBuffer_.size()) {
			const FastCgi::Record *record =
				reinterpret_cast<const FastCgi::Record *>(readBuffer_.data() + readOffset_);

			// payload fully available?
			if (readBuffer_.size() - readOffset_ < record->size())
				break;

			readOffset_ += record->size();

			TRACE(1, "Processing received FastCGI packet (%s).", record->type_str());

			if (!processRecord(record))
				goto done;
		}
	}

	if (revents & x0::Socket::Write) {
		ssize_t rv = socket_->write(writeBuffer_.ref(writeOffset_));

		if (rv < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				log(x0::Severity::error, "Writing to backend %s failed: %s", backendName_.c_str(), strerror(errno));
				goto app_err;
			}

			goto done;
		}

		writeOffset_ += rv;

		TRACE(1, "Wrote %ld bytes to upstream server.", rv);

		// if set watcher back to EV_READ if the write-buffer has been fully written (to catch connection close events)
		if (writeOffset_ == writeBuffer_.size()) {
			TRACE(1, "Pending write-buffer fully flushed to upstraem server.");
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
		TRACE(1, "Registering client-write-complete-callback.");
		writeCount_ = 0;
		socket_->setMode(x0::Socket::None);
		ref(); // will be unref'd in completion-handler, onWriteComplete().
		rn_->request->writeCallback<FastCgiTransport, &FastCgiTransport::onWriteComplete>(this);
	}

	unref();
}

void FastCgiTransport::onTimeout(x0::Socket* s)
{
	log(x0::Severity::error, "I/O timeout to backend %s: %s", backendName_.c_str(), strerror(errno));

	if (!rn_->request->status)
		rn_->request->status = HttpStatus::GatewayTimeout;

	backend_->setState(HealthState::Offline);
	close();
}

bool FastCgiTransport::processRecord(const FastCgi::Record *record)
{
	TRACE(1, "processRecord(type=%s (%d), rid=%d, contentLength=%d, paddingLength=%d)",
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
			log(x0::Severity::error,
				"Unknown transport record received from backend %s. type:%d, payload-size:%ld",
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
	TRACE(1, "Received protocol parameter %s=%s.", name.c_str(), value.c_str());
}

void FastCgiTransport::abortRequest()
{
	// TODO: install deadline-timer to actually close the connection if not done by the backend.
	isAborted_ = true;
	if (socket_->isOpen()) {
		write<FastCgi::AbortRequestRecord>(id_);
		flush();
	}
}

void FastCgiTransport::onStdOut(const x0::BufferRef& chunk)
{
	TRACE(1, "Received %ld bytes from upstream server (state=%s).", chunk.size(), state_str());
//	TRACE(2, "data: %s", chunk.str().c_str());
	process(chunk);
}

inline std::string chomp(const std::string& value) // {{{
{
	if (value.size() && value[value.size() - 1] == '\n')
		return value.substr(0, value.size() - 1);
	else
		return value;
} // }}}

void FastCgiTransport::onStdErr(const x0::BufferRef& chunk)
{
	log(x0::Severity::error, "%s", chomp(chunk.str()).c_str());
}

void FastCgiTransport::onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus)
{
	TRACE(1, "Received EndRequest-event from upstream server (appStatus=%d protocolStatus=%d). Closing transport.",
		appStatus, static_cast<int>(protocolStatus));

	auto r = rn_->request;
	if (!r->status) {
		switch (protocolStatus) {
			case FastCgi::ProtocolStatus::RequestComplete:
				r->status = HttpStatus::Ok;
				break;
			case FastCgi::ProtocolStatus::CannotMpxConnection:
				log(Severity::error, "Backend appliation terminated request because it says it cannot multiplex connections.");
				r->status = HttpStatus::InternalServerError;
				break;
			case FastCgi::ProtocolStatus::Overloaded:
				log(Severity::error, "Backend appliation terminated request because it says it is overloaded.");
				r->status = HttpStatus::ServiceUnavailable;
				break;
			case FastCgi::ProtocolStatus::UnknownRole:
				log(Severity::error, "Backend appliation terminated request because it cannot handle this role.");
				r->status = HttpStatus::InternalServerError;
				break;
			default:
				log(Severity::error, "Backend appliation terminated request with unknown error code %d.", static_cast<int>(protocolStatus));
				r->status = HttpStatus::InternalServerError;
				break;
		}
	}

	close();
}

void FastCgiTransport::processRequestBody(const x0::BufferRef& chunk)
{
	TRACE(1, "Received %ld / %ld bytes from client body.",
		chunk.size(), rn_->request->connection.contentLength());

	// if chunk.size() is 0, this also marks the fcgi stdin stream's end. so just pass it.
	write(FastCgi::Type::StdIn, id_, chunk.data(), chunk.size());

	flush();
}

bool FastCgiTransport::onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value)
{
	TRACE(1, "parsed HTTP header from upstream server. %s: %s",
		name.str().c_str(), value.str().c_str());

	if (x0::iequals(name, "Status")) {
		int status = value.ref(0, value.find(' ')).toInt();
		rn_->request->status = static_cast<x0::HttpStatus>(status);
	} else if (x0::iequals(name, "X-Sendfile")) {
		sendfile_ = value.str();
	} else {
		if (name == "Location")
			rn_->request->status = x0::HttpStatus::MovedTemporarily;

		rn_->request->responseHeaders.push_back(name.str(), value.str());
	}

	return true;
}

bool FastCgiTransport::onMessageHeaderEnd()
{
	if (unlikely(!sendfile_.empty())) {
		auto r = rn_->request;
		r->responseHeaders.remove("Content-Type");
		r->responseHeaders.remove("Content-Length");
		r->responseHeaders.remove("ETag");
		r->sendfile(sendfile_);
	}

	return true;
}

bool FastCgiTransport::onMessageContent(const x0::BufferRef& chunk)
{
	auto r = rn_->request;

	TRACE(1, "Parsed HTTP message content of %ld bytes from upstream server.", chunk.size());
	//TRACE(2, "Message content chunk: %s", chunk.str().c_str());

	if (unlikely(!sendfile_.empty()))
		// we ignore the backend's message body as we've replaced it with the file contents of X-Sendfile's file.
		return true;

	switch (backend_->manager()->transferMode()) {
	case TransferMode::FileAccel:
		if (!(transferHandle_ < 0)) {
			ssize_t rv = ::write(transferHandle_, chunk.data(), chunk.size());
			if (rv == static_cast<ssize_t>(chunk.size())) {
				r->write<FileSource>(transferHandle_, transferOffset_, rv, false);
				transferOffset_ += rv;
				break;
			} else if (rv > 0) {
				// partial write to disk (is this possible?) -- TODO: investigate if that case is possible
				transferOffset_ += rv;
			}
		}
		// fall through
		break;
	case TransferMode::MemoryAccel:
		r->write<x0::BufferRefSource>(chunk);
		break;
	case TransferMode::Blocking:
		r->write<x0::BufferRefSource>(chunk);

		// if the above write() operation did not complete and thus
		// we have data pending to be sent out to the client,
		// we need to install a completion callback once
		// all (possibly proceeding write operations) have been
		// finished within a single io()-callback run.

		if (r->connection.isOutputPending())
			++writeCount_;

		break;
	}

	return false;
}

void FastCgiTransport::log(x0::LogMessage&& msg)
{
	if (rn_) {
		msg.addTag("fastcgi/%d", transportId_);
		rn_->request->log(std::move(msg));
	}
}

template<typename... Args>
inline void FastCgiTransport::log(Severity severity, const char* fmt, Args&&... args)
{
	log(LogMessage(severity, fmt, args...));
}

/**
 * write-completion hook, invoked when a content chunk is written to the HTTP client.
 */
void FastCgiTransport::onWriteComplete()
{
#if 0//{{{
	TRACE(1, "FastCgiTransport.onWriteComplete() bufferSize: %ld", writeBuffer_.size());

	if (writeBuffer_.size() != 0) {
		TRACE(1, "onWriteComplete: queued:%ld", writeBuffer_.size());

		auto r = rn_->request;

		r->write<x0::BufferSource>(std::move(writeBuffer_));

		if (r->connection.isOutputPending()) {
			TRACE(1, "onWriteComplete: output pending. enqueue callback");
			r->writeCallback<FastCgiTransport, &FastCgiTransport::onWriteComplete>(this);
			return;
		}
	}
#endif//}}}
	TRACE(1, "onWriteComplete: output flushed. resume watching on app I/O (read)");

	if (socket_->isOpen()) {
		// the connection to the backend may already have been closed here when
		// we sent out BIG data to the client and the upstream server has issued an EndRequest-event already,
		// which causes a close() on this object and thus closes the connection to
		// the upstream server already, even though not all data has been flushed out to the client yet.

		TRACE(1, "Writing to client completed. Resume watching on app I/O for read.");
		socket_->setTimeout<FastCgiTransport, &FastCgiTransport::onTimeout>(this, backend_->manager()->readTimeout());
		socket_->setMode(x0::Socket::Read);
	} else {
		TRACE(1, "Writing to client completed (Upstream connection already closed).");
	}

	// unref the ref(), invoked near the installer code of this callback
	unref();
}

/**
 * Invoked when remote client connected before the response has been fully transmitted.
 */
void FastCgiTransport::onClientAbort(void *p)
{
	FastCgiTransport* self = reinterpret_cast<FastCgiTransport*>(p);

	self->log(x0::Severity::error, "Client closed connection early. Aborting request to upstream server.");

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

FastCgiBackend::FastCgiBackend(BackendManager* bm, const std::string& name, const SocketSpec& socketSpec, size_t capacity, bool healthChecks) :
	Backend(bm, name, socketSpec, capacity, healthChecks ? new FastCgiHealthMonitor(*bm->worker()->server().nextWorker()) : nullptr)
{
#ifndef XZERO_NDEBUG
	setLoggingPrefix("FastCgiBackend/%s", name.c_str());
#endif

	if (healthChecks) {
		healthMonitor()->setBackend(this);
	}
}

FastCgiBackend::~FastCgiBackend()
{
}

void FastCgiBackend::setup(const x0::SocketSpec& spec)
{
#ifndef XZERO_NDEBUG
	setLoggingPrefix("FastCgiBackend/%s", spec.str().c_str());
#endif
	socketSpec_ = spec;
}

const std::string& FastCgiBackend::protocol() const
{
	static const std::string value("fastcgi");
	return value;
}

bool FastCgiBackend::process(RequestNotes* rn)
{
	//TRACE(1, "process()");

	if (x0::Socket* socket = x0::Socket::open(rn->request->connection.worker().loop(), socketSpec_, O_NONBLOCK | O_CLOEXEC)) {
		if (++nextID_ == 0)
			++nextID_;

		new FastCgiTransport(this, rn, nextID_, socket);
		return true;
	} else {
		rn->request->log(x0::Severity::notice, "fastcgi: connection to backend %s failed (%d). %s", socketSpec_.str().c_str(), errno, strerror(errno));
		return false;
	}
}

/**
 * \brief enqueues this transport connection ready for serving the next request.
 * \param transport the transport connection object
 */
void FastCgiBackend::release(FastCgiTransport *transport)
{
	//TRACE(1, "FastCgiBackend.release()");
	delete transport;
}
//}}}
