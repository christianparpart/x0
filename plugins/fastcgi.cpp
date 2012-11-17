/* <x0/plugins/fastcgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: content generator
 *
 * description:
 *     Produces a response based on the specified FastCGI backend.
 *     This backend is communicating  via TCP/IP.
 *     Plans to support named sockets exist, but you may
 *     jump in and contribute aswell.
 *
 * compliance:
 *     This plugin will support FastCGI protocol, however, it will not
 *     try to multiplex multiple requests over a single FastCGI transport
 *     connection, that is, a single TCP socket.
 *     Instead, it will open a new transport connection for each parallel
 *     request, which makes things alot easier.
 *
 *     The FastCGI application will also be properly notified on
 *     early client aborts by either EndRecord packet or by a closed
 *     transport connection, both events denote the fact, that the client
 *     somehow is or gets disconnected to or by the server.
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     handler fastcgi(string host_and_port);                 # e.g. ("127.0.0.1:3000")
 *     __todo handler fastcgi(ip host, int port)              # e.g. (127.0.0.1, 3000)
 *     __todo handler fastcgi(sequence of [ip host, int port])# e.g. (1.2.3.4, 988], [1.2.3.5, 988])
 *
 * todo:
 *     - error handling, including:
 *       - XXX early http client abort (raises EndRequestRecord-submission to application)
 *       - log stream parse errors,
 *       - transport level errors (connect/read/write errors)
 *       - timeouts
 */

#include "fastcgi_protocol.h"

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpMessageProcessor.h>
#include <x0/io/BufferSource.h>
#include <x0/io/BufferRefSource.h>
#include <x0/SocketSpec.h>
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

// TODO make these values configurable
#define FASTCGI_CONNECT_TIMEOUT x0::TimeSpan::fromSeconds(60)  /* fastcgi.connect_idle */
#define FASTCGI_READ_TIMEOUT x0::TimeSpan::fromSeconds(300)    /* fastcgi.read_idle */
#define FASTCGI_WRITE_TIMEOUT x0::TimeSpan::fromSeconds(60)    /* fastcgi.write_idle */

class CgiContext;
class CgiTransport;

inline std::string chomp(const std::string& value) // {{{
{
	if (value.size() && value[value.size() - 1] == '\n')
		return value.substr(0, value.size() - 1);
	else
		return value;
} // }}}

std::atomic<unsigned long long> transportIds_(0);

class CgiTransport : // {{{
	public x0::HttpMessageProcessor
#ifndef NDEBUG
	, public x0::Logging
#endif
{
	class ParamReader : public FastCgi::CgiParamStreamReader //{{{
	{
	private:
		CgiTransport *tx_;

	public:
		explicit ParamReader(CgiTransport *tx) : tx_(tx) {}

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
	CgiContext *context_;

	uint16_t id_;
	std::string backendName_;
	x0::Socket* backend_;

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
	explicit CgiTransport(CgiContext* cx, x0::HttpRequest* r, uint16_t id, x0::Socket* backend);
	~CgiTransport();

	void ref();
	void unref();

	void bind();
	void close();

	// server-to-application
	void abortRequest();

	// application-to-server
	void onStdOut(const x0::BufferRef& chunk);
	void onStdErr(const x0::BufferRef& chunk);
	void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

	CgiContext& context() const { return *context_; }

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
	void timeout(x0::Socket* s);

	inline bool processRecord(const FastCgi::Record *record);
	inline void onParam(const std::string& name, const std::string& value);

	void inspect(x0::Buffer& out);

	template<typename... Args>
	void log(x0::Severity severity, const char* fmt, Args&&... args)
	{
		if (request_) {
			x0::Buffer msg;
			msg.push_back("fastcgi/");
			msg.push_back(transportId_);
			msg.push_back(": ");
			msg.push_back(fmt);

			request_->log(severity, msg.c_str(), args...);
		}
	}

}; // }}}

class CgiContext //{{{
#ifndef NDEBUG
	: public x0::Logging
#endif
{
public:
	static uint16_t nextID_;

	x0::HttpServer& server_;
	x0::SocketSpec spec_;

public:
	CgiContext(x0::HttpServer& server);
	~CgiContext();

	x0::HttpServer& server() const { return server_; }
	void setup(const x0::SocketSpec& spec);

	void handleRequest(x0::HttpRequest *in);

	void release(CgiTransport *transport);
};
// }}}

// {{{ CgiTransport impl
CgiTransport::CgiTransport(CgiContext* cx, x0::HttpRequest* r, uint16_t id, x0::Socket* backend) :
	HttpMessageProcessor(x0::HttpMessageProcessor::MESSAGE),
	transportId_(++transportIds_),
	refCount_(1),
	isAborted_(false),
	context_(cx),
	id_(id),
	backendName_(backend->remote()),
	backend_(backend),
	readBuffer_(),
	readOffset_(0),
	writeBuffer_(),
	writeOffset_(0),
	flushPending_(false),
	configured_(false),

	request_(r),
	paramWriter_(),
	writeCount_(0)
{
#ifndef NDEBUG
	static std::atomic<int> mi(0);
	setLoggingPrefix("CgiTransport/%d", ++mi);
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

	bind();
}

CgiTransport::~CgiTransport()
{
	log(x0::Severity::debug, "closing transport connection to upstream server.");

	if (backend_) {
		if (backend_->isOpen())
			backend_->close();

		delete backend_;
	}

	if (request_) {
		if (request_->status == x0::HttpStatus::Undefined) {
			request_->status = x0::HttpStatus::ServiceUnavailable;
		}

		request_->finish();
	}
}

void CgiTransport::close()
{
	log(x0::Severity::debug3, "Closing transport connection.");

	if (backend_->isOpen()) {
		backend_->close();
	}

	unref(); // related to the increment in contructer CgiTransport()
}

void CgiTransport::ref()
{
	++refCount_;

	log(x0::Severity::debug4, "Incrementing reference count to %d.", refCount_);
}

void CgiTransport::unref()
{
	log(x0::Severity::debug4, "Decrementing reference count from %d.", refCount_);

	assert(refCount_ > 0);

	--refCount_;

	if (refCount_ == 0) {
		context_->release(this);
	}
}

void CgiTransport::bind()
{
	// initialize object
	request_->setAbortHandler(&CgiTransport::onClientAbort, this);

	request_->registerInspectHandler<CgiTransport, &CgiTransport::inspect>(this);

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

		request_->setBodyCallback<CgiTransport, &CgiTransport::processRequestBody>(this);
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
	if (backend_->state() == x0::Socket::Connecting) {
		backend_->setTimeout<CgiTransport, &CgiTransport::onConnectTimeout>(this, FASTCGI_CONNECT_TIMEOUT);
		backend_->setReadyCallback<CgiTransport, &CgiTransport::onConnectComplete>(this);
	} else
		backend_->setReadyCallback<CgiTransport, &CgiTransport::io>(this);

	// flush out
	flush();
}

template<typename T, typename... Args>
inline void CgiTransport::write(Args&&... args)
{
	T record(args...);
	write(&record);
}

inline void CgiTransport::write(FastCgi::Type type, int requestId, x0::Buffer&& content)
{
	write(type, requestId, content.data(), content.size());
}

void CgiTransport::write(FastCgi::Type type, int requestId, const char *buf, size_t len)
{
	const size_t chunkSizeCap = 0xFFFF;
	static const char padding[8] = { 0 };

	if (len == 0) {
		FastCgi::Record record(type, requestId, 0, 0);
		log(x0::Severity::debug, "writing packet (%s) of %ld bytes to upstream server.", record.type_str(), len);
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

		log(x0::Severity::debug, "writing packet (%s) of %ld bytes to upstream server.", record.type_str(), record.size());
	}
}

void CgiTransport::write(FastCgi::Record *record)
{
	log(x0::Severity::debug, "writing packet (%s) of %ld bytes to upstream server.", record->type_str(), record->size());

	writeBuffer_.push_back(record->data(), record->size());
}

void CgiTransport::flush()
{
	if (backend_->state() == x0::Socket::Operational) {
		log(x0::Severity::debug, "flushing pending data to upstream server.");
		backend_->setTimeout<CgiTransport, &CgiTransport::timeout>(this, FASTCGI_WRITE_TIMEOUT);
		backend_->setMode(x0::Socket::ReadWrite);
	} else {
		log(x0::Severity::debug, "mark pending data to be flushed to upstream server.");
		flushPending_ = true;
	}
}

void CgiTransport::onConnectTimeout(x0::Socket* s)
{
	log(x0::Severity::error, "Trying to connect to upstream server was timing out.");
	close();
}

/** invoked (by open() or asynchronousely by io()) to complete the connection establishment.
 * \retval true connection establishment completed
 * \retval false finishing connect() failed. object is also invalidated.
 */
void CgiTransport::onConnectComplete(x0::Socket* s, int revents)
{
	if (s->isClosed()) {
		log(x0::Severity::error, "connection to upstream server failed. %s", strerror(errno));
		request_->status = x0::HttpStatus::ServiceUnavailable;
		close();
	} else if (writeBuffer_.size() > writeOffset_ && flushPending_) {
		log(x0::Severity::debug, "Connected. Flushing pending data.");
		flushPending_ = false;
		backend_->setReadyCallback<CgiTransport, &CgiTransport::io>(this);
		backend_->setTimeout<CgiTransport, &CgiTransport::timeout>(this, FASTCGI_WRITE_TIMEOUT);
		backend_->setMode(x0::Socket::ReadWrite);
	} else {
		log(x0::Severity::debug, "Connected.");
		backend_->setReadyCallback<CgiTransport, &CgiTransport::io>(this);
		backend_->setTimeout<CgiTransport, &CgiTransport::timeout>(this, FASTCGI_READ_TIMEOUT); // FIXME I think we don't want to timeout-on-read here.
		backend_->setMode(x0::Socket::Read);
	}
}

void CgiTransport::io(x0::Socket* s, int revents)
{
	log(x0::Severity::debug3, "Received I/O activity on upstream socket. revents=0x%04x", revents);

	if (revents & ev::ERROR) {
		log(x0::Severity::error, "Internal error occured while waiting for I/O readiness from backend application.");
		close();
		return;
	}

	ref();

	if (revents & x0::Socket::Read) {
		log(x0::Severity::debug3, "reading from upstream server.");
		// read as much as possible
		for (;;) {
			size_t remaining = readBuffer_.capacity() - readBuffer_.size();
			if (remaining < 1024) {
				readBuffer_.reserve(readBuffer_.capacity() + 4*4096);
				remaining = readBuffer_.capacity() - readBuffer_.size();
			}

			int rv = backend_->read(readBuffer_);

			if (rv == 0) {
				if (request_->status == x0::HttpStatus::Undefined) {
					// we did not actually process any response though
					log(x0::Severity::error, "Connection to backend lost.");
				}
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

			log(x0::Severity::debug3, "Processing received FastCGI packet (%s).", record->type_str());

			if (!processRecord(record))
				goto done;
		}
	}

	if (revents & x0::Socket::Write) {
		ssize_t rv = backend_->write(writeBuffer_.ref(writeOffset_));

		if (rv < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				log(x0::Severity::error, "Writing to backend %s failed: %s", backendName_.c_str(), strerror(errno));
				goto app_err;
			}

			goto done;
		}

		writeOffset_ += rv;

		log(x0::Severity::debug3, "Wrote %ld bytes to upstream server.", rv);

		// if set watcher back to EV_READ if the write-buffer has been fully written (to catch connection close events)
		if (writeOffset_ == writeBuffer_.size()) {
			log(x0::Severity::debug3, "Pending write-buffer fully flushed to upstraem server.");
			backend_->setMode(x0::Socket::Read);
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
		log(x0::Severity::debug3, "Registering client-write-complete-callback.");
		writeCount_ = 0;
		backend_->setMode(x0::Socket::None);
		ref(); // will be unref'd in completion-handler, onWriteComplete().
		request_->writeCallback<CgiTransport, &CgiTransport::onWriteComplete>(this);
	}

	unref();
}

void CgiTransport::timeout(x0::Socket* s)
{
	log(x0::Severity::error, "I/O timeout to backend %s: %s", backendName_.c_str(), strerror(errno));

	close();
}

bool CgiTransport::processRecord(const FastCgi::Record *record)
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

void CgiTransport::onParam(const std::string& name, const std::string& value)
{
	log(x0::Severity::debug, "Received protocol parameter %s=%s.", name.c_str(), value.c_str());
}

void CgiTransport::abortRequest()
{
	// TODO: install deadline-timer to actually close the connection if not done by the backend.
	isAborted_ = true;
	if (backend_->isOpen()) {
		write<FastCgi::AbortRequestRecord>(id_);
		flush();
	}
}

void CgiTransport::onStdOut(const x0::BufferRef& chunk)
{
	log(x0::Severity::debug, "Received %ld bytes from upstream server (state=%s).", chunk.size(), state_str());

	process(chunk);
}

void CgiTransport::onStdErr(const x0::BufferRef& chunk)
{
	log(x0::Severity::error, "%s", chomp(chunk.str()).c_str());
}

void CgiTransport::onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus)
{
	log(x0::Severity::debug, "Received EndRequest-event from upstream server (appStatus=%d protocolStatus=%d). Closing transport.",
		appStatus, static_cast<int>(protocolStatus));

	close();
}

void CgiTransport::processRequestBody(const x0::BufferRef& chunk)
{
	log(x0::Severity::debug, "Received %ld / %ld bytes from client body.",
		chunk.size(), request_->connection.contentLength());

	// if chunk.size() is 0, this also marks the fcgi stdin stream's end. so just pass it.
	write(FastCgi::Type::StdIn, id_, chunk.data(), chunk.size());

	flush();
}

bool CgiTransport::onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value)
{
	log(x0::Severity::debug2, "parsed HTTP header from upstream server. %s: %s",
		name.str().c_str(), value.str().c_str());

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

bool CgiTransport::onMessageContent(const x0::BufferRef& content)
{
	log(x0::Severity::debug2, "Parsed HTTP message content of %ld bytes from upstream server.", content.size());

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

/** \brief write-completion hook, invoked when a content chunk is written to the HTTP client.
 */
void CgiTransport::onWriteComplete()
{
#if 0//{{{
	TRACE("CgiTransport.onWriteComplete() bufferSize: %ld", writeBuffer_.size());

	if (writeBuffer_.size() != 0) {
		TRACE("onWriteComplete: queued:%ld", writeBuffer_.size());

		request_->write<x0::BufferSource>(std::move(writeBuffer_));

		if (request_->connection.isOutputPending()) {
			TRACE("onWriteComplete: output pending. enqueue callback");
			request_->writeCallback<CgiTransport, &CgiTransport::onWriteComplete>(this);
			return;
		}
	}
#endif//}}}

	if (backend_->isOpen()) {
		// the connection to the backend may already have been closed here when
		// we sent out BIG data to the client and the upstream server has issued an EndRequest-event already,
		// which causes a close() on this object and thus closes the connection to
		// the upstream server already, even though not all data has been flushed out to the client yet.

		log(x0::Severity::debug3, "Writing to client completed. Resume watching on app I/O for read.");
		backend_->setTimeout<CgiTransport, &CgiTransport::timeout>(this, FASTCGI_READ_TIMEOUT);
		backend_->setMode(x0::Socket::Read);
	} else {
		log(x0::Severity::debug3, "Writing to client completed (Upstream connection already closed).");
	}

	// unref the ref(), invoked near the installer code of this callback
	unref();
}

/**
 * @brief invoked when remote client connected before the response has been fully transmitted.
 *
 * @param p `this pointer` to CgiTransport object
 */
void CgiTransport::onClientAbort(void *p)
{
	CgiTransport* self = reinterpret_cast<CgiTransport*>(p);

	self->log(x0::Severity::error, "Client closed connection early. Aborting request to upstream server.");

	// notify fcgi app about client abort
	self->abortRequest();
}

void CgiTransport::inspect(x0::Buffer& out)
{
	//out << "Hello, World<br/>";
	out << "fcgi.refcount:" << refCount_ << ", ";
	out << "aborted:" << isAborted_ << ", ";
	backend_->inspect(out);
}
// }}}

// {{{ CgiContext impl
uint16_t CgiContext::nextID_ = 0;

CgiContext::CgiContext(x0::HttpServer& server) :
	server_(server),
	spec_()
{
}

CgiContext::~CgiContext()
{
}

void CgiContext::setup(const x0::SocketSpec& spec)
{
#ifndef NDEBUG
	setLoggingPrefix("CgiContext(%s)", spec.str().c_str());
#endif
	spec_ = spec;
}

void CgiContext::handleRequest(x0::HttpRequest *in)
{
	TRACE("CgiContext.handleRequest()");

	x0::Socket* backend = new x0::Socket(in->connection.worker().loop());
	backend->open(spec_, O_NONBLOCK | O_CLOEXEC);

	if (backend->isOpen()) {
		if (++nextID_ == 0)
			++nextID_;

		new CgiTransport(this, in, nextID_, backend);
	} else {
		in->log(x0::Severity::error, "Connection to backend %s failed: %s",
			spec_.str().c_str(), strerror(errno));
		in->status = x0::HttpStatus::ServiceUnavailable;
		in->finish();

		delete backend;
	}
}

/**
 * \brief enqueues this transport connection ready for serving the next request.
 * \param transport the transport connection object
 */
void CgiContext::release(CgiTransport *transport)
{
	TRACE("CgiContext.release()");
	delete transport;
}
//}}}

// {{{ FastCgiPlugin
/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class FastCgiPlugin :
	public x0::HttpPlugin
{
public:
	FastCgiPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<FastCgiPlugin, &FastCgiPlugin::handleRequest>("fastcgi");
	}

	~FastCgiPlugin()
	{
		for (auto i: contexts_)
			delete i.second;
	}

	bool handleRequest(x0::HttpRequest *in, const x0::FlowParams& args)
	{
		x0::SocketSpec spec;
		spec << args;

		if (!spec.isValid() || spec.backlog() >= 0) {
			in->log(x0::Severity::error, "Invalid socket spec passed.");
			return false;
		}

		CgiContext *cx = acquireContext(spec);
		if (!cx)
			return false;

		cx->handleRequest(in);
		return true;
	}

	CgiContext *acquireContext(const x0::SocketSpec& spec)
	{
#if 0
		auto i = contexts_.find(app);
		if (i != contexts_.end()) {
			//TRACE("acquireContext('%s') available.", app.c_str());
			return i->second;
		}
#endif
		CgiContext *cx = new CgiContext(server());
		cx->setup(spec);
		//contexts_[app] = cx;
		//TRACE("acquireContext('%s') spawned (%ld).", app.c_str(), contexts_.size());
		return cx;
	}

private:
	std::unordered_map<std::string, CgiContext *> contexts_;
}; // }}}

X0_EXPORT_PLUGIN_CLASS(FastCgiPlugin)
