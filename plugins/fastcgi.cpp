/* <x0/plugins/fastcgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
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
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#if 1 // !defined(NDEBUG)
#	define TRACE(msg...) DEBUG("fastcgi: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif

// X0_FASTCGI_DIRECT_IO
//     if this variable is defined, all received response data is being pushed 
//     to the HTTP client as soon as possible.
//
//     If not defined, the client will only receive the content once
//     *all* data from the backend application has been arrived in x0.
//
#define X0_FASTCGI_DIRECT_IO (1)

class CgiContext;
class CgiTransport;

inline std::string chomp(const std::string& value) // {{{
{
	if (value.size() && value[value.size() - 1] == '\n')
		return value.substr(0, value.size() - 1);
	else
		return value;
} // }}}

class CgiTransport : // {{{
	public x0::HttpMessageProcessor
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
	enum State {
		DISCONNECTED,
		CONNECTING,
		CONNECTED
	};

	ev_io io_;

	CgiContext *context_;

	struct ev_loop *loop_;

	std::string hostname_;
	int port_;
	uint16_t id_;

	int fd_;
	State state_;

	x0::Buffer readBuffer_;
	size_t readOffset_;
	x0::Buffer writeBuffer_;
	size_t writeOffset_;
	bool flushPending_;

	bool configured_;

	// aka CgiRequest
	x0::HttpRequest *request_;
	FastCgi::CgiParamStreamWriter paramWriter_;

#if X0_FASTCGI_DIRECT_IO
	bool writeActive_;
	bool finish_;
#endif

public:
	explicit CgiTransport(CgiContext *cx);
	~CgiTransport();

	State state() const { return state_; }

	bool open(const char *hostname, int port);
	void bind(x0::HttpRequest *in, uint16_t id);

	// server-to-application
	void beginRequest();
	void streamParams();
	void abortRequest();

	// application-to-server
	void onStdOut(x0::BufferRef&& chunk);
	void onStdErr(x0::BufferRef&& chunk);
	void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

	CgiContext& context() const { return *context_; }

public:
	template<typename T, typename... Args> void write(Args&&... args)
	{
		T *record = new T(args...);
		write(record);
		delete record;
	}

	void write(FastCgi::Type type, int requestId, x0::Buffer&& content);
	void write(FastCgi::Type type, int requestId, const char *buf, size_t len);
	void write(FastCgi::Record *record);
	void flush();

private:
	void processRequestBody(x0::BufferRef&& chunk);

	virtual void messageHeader(x0::BufferRef&& name, x0::BufferRef&& value);
	virtual bool messageContent(x0::BufferRef&& content);
	void writeComplete(int error, size_t nwritten);
	static void onClientAbort(void *p);

	void finish();

	static void io_thunk(struct ev_loop *, ev_io *w, int revents);

	inline bool onConnectComplete();
	inline void io(int revents);
	inline bool processRecord(const FastCgi::Record *record);
	inline void onParam(const std::string& name, const std::string& value);
}; // }}}

class CgiContext //{{{
{
public:
	static uint16_t nextID_;

	x0::HttpServer& server_;
	std::string host_;
	int port_;

public:
	CgiContext(x0::HttpServer& server);
	~CgiContext();

	x0::HttpServer& server() const { return server_; }
	void setup(const std::string& application);

	bool handleRequest(x0::HttpRequest *in);

	void release(CgiTransport *transport);
};
// }}}

// {{{ CgiTransport impl
CgiTransport::CgiTransport(CgiContext *cx) :
	HttpMessageProcessor(x0::HttpMessageProcessor::MESSAGE),
	io_(),
	context_(cx),
	loop_(ev_default_loop(0)),
	hostname_(),
	port_(0),
	id_(1),
	fd_(-1),
	state_(DISCONNECTED),
	readBuffer_(),
	readOffset_(0),
	writeBuffer_(),
	writeOffset_(0),
	flushPending_(false),
	configured_(false),

	request_(nullptr),
	paramWriter_()
#if X0_FASTCGI_DIRECT_IO
	, writeActive_(false)
	, finish_(false)
#endif
{
	TRACE("CgiTransport()");
	ev_init(&io_, &CgiTransport::io_thunk);
	io_.data = this;

	// stream management record: GetValues
#if 0
	FastCgi::CgiParamStreamWriter mr;
	mr.encode("FCGI_MPXS_CONNS", "");
	mr.encode("FCGI_MAX_CONNS", "");
	mr.encode("FCGI_MAX_REQS", "");
	write(FastCgi::Type::GetValues, 0, mr.output());
#endif
}

CgiTransport::~CgiTransport()
{
	TRACE("~CgiTransport()");

	assert(request_ == nullptr);

	ev_io_stop(loop_, &io_);
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = -1;
	}
}

/** asynchronousely connects to given hostname:port.
 *
 * \retval true connected synchronously or asynchronousely (not completed yet) in success
 * \retval false something went wrong. if so, the underlying object is to be destructed already, too.
 */
bool CgiTransport::open(const char *hostname, int port)
{
	TRACE("connect(hostname=%s, port=%d)", hostname, port);

	hostname_ = hostname;
	port_ = port;

	struct addrinfo hints;
	struct addrinfo *res;
	bool result = false;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char sport[16];
	snprintf(sport, sizeof(sport), "%d", port);

	int rv = getaddrinfo(hostname, sport, &hints, &res);
	if (rv) {
		context().server().log(x0::Severity::error,
				"fastcgi: could not get addrinfo of %s:%s: %s", 
				hostname, sport, gai_strerror(rv));
		return false;
	}

	for (struct addrinfo *rp = res; rp != nullptr; rp = rp->ai_next) {
		fd_ = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd_ < 0) {
			context().server().log(x0::Severity::error,
					"fastcgi: socket creation error: %s",  strerror(errno));
			continue;
		}

		fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK);
		fcntl(fd_, F_SETFD, fcntl(fd_, F_GETFD) | FD_CLOEXEC);

		rv = ::connect(fd_, rp->ai_addr, rp->ai_addrlen);
		if (rv == 0) {
			TRACE("connect: instant success (fd:%d)", fd_);
			result = onConnectComplete();
			break;
		} else if (/*rv < 0 &&*/ errno == EINPROGRESS) {
			TRACE("connect: backgrounding (fd:%d)", fd_);

			state_ = CONNECTING;
			ev_io_set(&io_, fd_, EV_WRITE);
			ev_io_start(loop_, &io_);

			result = true;
			break;
		} else {
			context().server().log(x0::Severity::error,
				"fastcgi: could not connect to %s:%s: %s", hostname_.c_str(), sport, strerror(errno));
			::close(fd_);
			fd_ = -1;
		}
	}

	freeaddrinfo(res);
	return result;
}

void CgiTransport::bind(x0::HttpRequest *in, uint16_t id)
{
	assert(request_ == nullptr);

	request_ = in;
	id_ = id;

	beginRequest();
	streamParams();

	flush();
}

inline void CgiTransport::write(FastCgi::Type type, int requestId, x0::Buffer&& content)
{
	write(type, requestId, content.data(), content.size());
}

void CgiTransport::write(FastCgi::Type type, int requestId, const char *buf, size_t len)
{
	TRACE("CgiTransport.write(rid=%d, body-size=%ld)", requestId, len);

#if 0
	FastCgi::Record record(type, requestId, content.size(), 0);
	writeBuffer_.push_back(record.data(), sizeof(record));
	writeBuffer_.push_back(buf, len);
	x0::Buffer::dump(buf, len, "CHUNK");

	TRACE("-CgiTransport.write(type=%s, rid=%d, size=%d, pad=%d)",
			record.type_str(), record.requestId(), record.size(), record.paddingLength());
#else
	if (len == 0) {
		FastCgi::Record record(type, requestId, 0, 0);
		writeBuffer_.push_back(record.data(), sizeof(record));
		return;
	}

	const size_t chunkSize = 0xFFFF;

	for (size_t offset = 0; offset < len; offset += chunkSize) {
		size_t clen = std::min(offset + chunkSize, len) - offset;

		FastCgi::Record record(type, requestId, clen, 0);
		writeBuffer_.push_back(record.data(), sizeof(record));
		writeBuffer_.push_back(buf + offset, clen);
		//x0::Buffer::dump(buf + offset, clen, "CHUNK");

		TRACE("-CgiTransport.write(type=%s, rid=%d, offset=%ld, size=%ld)", 
				record.type_str(), requestId, offset, clen);
	}
#endif
}

void CgiTransport::write(FastCgi::Record *record)
{
	TRACE("CgiTransport.write(type=%s, rid=%d, size=%d, pad=%d)", 
			record->type_str(), record->requestId(), record->size(), record->paddingLength());

	writeBuffer_.push_back(record->data(), record->size());
}

void CgiTransport::flush()
{
	if (state_ == CONNECTED && writeBuffer_.size() != 0 && writeOffset_ == 0)
	{
		TRACE("CgiTransport.flush() -> ev (fd:%d)", fd_);
		ev_io_stop(loop_, &io_);
		ev_io_set(&io_, fd_, EV_READ | EV_WRITE);
		ev_io_start(loop_, &io_);
	}
	else
	{
		TRACE("CgiTransport.flush() -> pending");
		flushPending_ = true;
	}
}

void CgiTransport::io_thunk(struct ev_loop *, ev_io *w, int revents)
{
	CgiTransport *cc = reinterpret_cast<CgiTransport *>(w->data);

	if (cc->state_ == CONNECTING && revents & EV_WRITE)
		cc->onConnectComplete();
	else
		cc->io(revents);
}

/** invoked (by open() or asynchronousely by io()) to complete the connection establishment.
 * \retval true connection establishment completed
 * \retval false finishing connect() failed. object is also invalidated.
 */
bool CgiTransport::onConnectComplete()
{
	int val = 0;
	socklen_t vlen = sizeof(val);
	if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
		if (val == 0) {
			TRACE("connect: success");
		} else {
			context().server().log(x0::Severity::error,
				"fastcgi: could not connect to %s:%d: %s", hostname_.c_str(), port_, strerror(val));
			finish();
			return false;
		}
	} else {
		context().server().log(x0::Severity::error,
			"fastcgi: could not getsocketopt(SO_ERROR) to %s:%d: %s", hostname_.c_str(), port_, strerror(errno));
		finish();
		return false;
	}

	// connect successful
	state_ = CONNECTED;

	if (writeBuffer_.size() > writeOffset_)
	{
		TRACE("CgiTransport.connected() flush pending");
		flushPending_ = false;

		ev_io_set(&io_, fd_, EV_READ | EV_WRITE);
		ev_io_start(loop_, &io_);
	}
	else
	{
		TRACE("CgiTransport.connected()");
		ev_io_set(&io_, fd_, EV_READ);
		ev_io_start(loop_, &io_);
	}

	return true;
}

void CgiTransport::io(int revents)
{
	TRACE("CgiTransport::io(0x%04x)", revents);
	if (revents & EV_READ)
	{
		TRACE("CgiTransport::io(): reading ...");
		// read as much as possible
		for (;;)
		{
			size_t remaining = readBuffer_.capacity() - readBuffer_.size();
			if (remaining < 1024) {
				readBuffer_.reserve(readBuffer_.capacity() + 4096);
				remaining = readBuffer_.capacity() - readBuffer_.size();
			}

			ssize_t rv = ::read(fd_, const_cast<char *>(readBuffer_.data() + readBuffer_.size()), remaining);

			if (rv == 0) {
				TRACE("fastcgi: connection to backend lost.");
				finish();
				return;
			}

			if (rv < 0) {
				if (errno != EINTR && errno != EAGAIN)
					context().server().log(x0::Severity::error,
						"fastcgi: read from backend %s:%d failed: %s",
						hostname_.c_str(), port_, strerror(errno));

				break;
			}

			readBuffer_.resize(readBuffer_.size() + rv);
		}

		// process fully received records
		TRACE("CgiTransport::io(): processing ...");
		while (readOffset_ + sizeof(FastCgi::Record) < readBuffer_.size())
		{
			const FastCgi::Record *record =
				reinterpret_cast<const FastCgi::Record *>(readBuffer_.data() + readOffset_);

			// payload fully available?
			if (readBuffer_.size() - readOffset_ < record->size())
				break;

			readOffset_ += record->size();

			if (!processRecord(record))
				return;
		}
	}

	if (revents & EV_WRITE)
	{
		TRACE("CgiTransport::io(): writing ...");
		//size_t sz = writeBuffer_.size() - writeOffset_;
		//TRACE("CgiTransport.write(fd:%d): %ld -> %ld\n", fd_, sz, rv);
		ssize_t rv = ::write(fd_, writeBuffer_.data() + writeOffset_, writeBuffer_.size() - writeOffset_);
		TRACE("CgiTransport::io(): write() -> %ld ...", rv);

		if (rv < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				context().server().log(x0::Severity::error,
					"fastcgi: write to backend %s:%d failed: %s", hostname_.c_str(), port_, strerror(errno));
			}

			return;
		}

		writeOffset_ += rv;

		// if set watcher back to EV_READ if the write-buffer has been fully written (to catch connection close events)
		if (writeOffset_ == writeBuffer_.size()) {
			TRACE("CgiTransport::io(): write buffer fully written to socket (%ld)", writeOffset_);
			ev_io_stop(loop_, &io_);
			ev_io_set(&io_, fd_, EV_READ);
			ev_io_start(loop_, &io_);

			writeBuffer_.clear();
			writeOffset_ = 0;
		}
	}
}

bool CgiTransport::processRecord(const FastCgi::Record *record)
{
	TRACE("processRecord(type=%s (%d), rid=%d, contentLength=%d, paddingLength=%d)",
		record->type_str(), record->type(), record->requestId(),
		record->contentLength(), record->paddingLength());

	bool proceedHint = true;

	switch (record->type())
	{
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
			context().server().log(x0::Severity::error,
				"fastcgi: unknown transport record received from backend %s:%d. type:%d, payload-size:%ld",
				hostname_.c_str(), port_, record->type(), record->contentLength());
			break;
	}
	return proceedHint;
}

void CgiTransport::onParam(const std::string& name, const std::string& value)
{
	TRACE("onParam(%s, %s)", name.c_str(), value.c_str());
}

void CgiTransport::beginRequest()
{
	write<FastCgi::BeginRequestRecord>(FastCgi::Role::Responder, id_, true);
}

void CgiTransport::streamParams()
{
	paramWriter_.encode("SERVER_SOFTWARE", PACKAGE_NAME "/" PACKAGE_VERSION);
	paramWriter_.encode("SERVER_NAME", request_->requestHeader("Host"));
	paramWriter_.encode("GATEWAY_INTERFACE", "CGI/1.1");

	paramWriter_.encode("SERVER_PROTOCOL", "1.1");
	paramWriter_.encode("SERVER_ADDR", request_->connection.localIP());
	paramWriter_.encode("SERVER_PORT", boost::lexical_cast<std::string>(request_->connection.localPort()));// TODO this should to be itoa'd only ONCE

	paramWriter_.encode("REQUEST_METHOD", request_->method);
	paramWriter_.encode("REDIRECT_STATUS", "200"); // for PHP configured with --force-redirect (Gentoo/Linux e.g.)

	request_->updatePathInfo(); // should we invoke this explicitely? I'd vote for no... however.

	paramWriter_.encode("SCRIPT_NAME", request_->path);
	paramWriter_.encode("PATH_INFO", request_->pathinfo);
	if (!request_->pathinfo.empty())
		paramWriter_.encode("PATH_TRANSLATED", request_->documentRoot, request_->pathinfo);

	paramWriter_.encode("QUERY_STRING", request_->query);			// unparsed uri
	paramWriter_.encode("REQUEST_URI", request_->uri);

	//paramWriter_.encode("REMOTE_HOST", "");  // optional
	paramWriter_.encode("REMOTE_ADDR", request_->connection.remoteIP());
	paramWriter_.encode("REMOTE_PORT", boost::lexical_cast<std::string>(request_->connection.remotePort()));

	//paramWriter_.encode("AUTH_TYPE", ""); // TODO
	//paramWriter_.encode("REMOTE_USER", "");
	//paramWriter_.encode("REMOTE_IDENT", "");

	if (request_->contentAvailable()) {
		paramWriter_.encode("CONTENT_TYPE", request_->requestHeader("Content-Type"));
		paramWriter_.encode("CONTENT_LENGTH", request_->requestHeader("Content-Length"));

		request_->read(std::bind(&CgiTransport::processRequestBody, this, std::placeholders::_1));
	}

#if defined(WITH_SSL)
	if (request_->connection.isSecure())
		paramWriter_.encode("HTTPS", "1");
#endif

	// HTTP request headers
	for (auto i = request_->requestHeaders.begin(), e = request_->requestHeaders.end(); i != e; ++i)
	{
		std::string key;
		key.reserve(5 + i->name.size());
		key += "HTTP_";

		for (auto p = i->name.begin(), q = i->name.end(); p != q; ++p)
			key += std::isalnum(*p) ? std::toupper(*p) : '_';

		paramWriter_.encode(key, i->value);
	}
	paramWriter_.encode("DOCUMENT_ROOT", request_->documentRoot);
	paramWriter_.encode("SCRIPT_FILENAME", request_->fileinfo->filename());

	write(FastCgi::Type::Params, id_, paramWriter_.output());
	write(FastCgi::Type::Params, id_, "", 0); // EOS
}

void CgiTransport::abortRequest()
{
	write<FastCgi::AbortRequestRecord>(id_);
	flush();
}

void CgiTransport::onStdOut(x0::BufferRef&& chunk)
{
	TRACE("CgiTransport.onStdOut(id:%d, chunk.size:%ld)", id_, chunk.size());
	size_t np = 0;
	process(std::move(chunk), np);
}

void CgiTransport::onStdErr(x0::BufferRef&& chunk)
{
	TRACE("CgiTransport.stderr(id:%d): %s", id_, chomp(chunk.str()).c_str());

	if (!request_)
		return;

	request_->log(x0::Severity::error, "fastcgi: %s", chomp(chunk.str()).c_str());
}

void CgiTransport::onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus)
{
#if X0_FASTCGI_DIRECT_IO
	TRACE("CgiTransport.onEndRequest(appStatus=%d, protocolStatus=%d) writeActive:%d", appStatus, (int)protocolStatus, writeActive_);
	if (writeActive_)
		finish_ = true;
	else
		finish();
#else
	TRACE("CgiTransport.onEndRequest(appStatus=%d, protocolStatus=%d)", appStatus, (int)protocolStatus);
	if (writeBuffer_.size() != 0)
		request_->write(
			std::make_shared<x0::BufferSource>(writeBuffer_),
			std::bind(&CgiTransport::writeComplete, this, std::placeholders::_1, std::placeholders::_2)
		);
	else
		finish();
#endif
}

void CgiTransport::processRequestBody(x0::BufferRef&& chunk)
{
	TRACE("CgiTransport.processRequestBody(chunkLen=%ld, (r)contentLen=%ld)", chunk.size(),
			request_->connection.contentLength());

	write(FastCgi::Type::StdIn, id_, chunk.data(), chunk.size());

	if (request_->connection.contentLength() > 0)
		request_->read(std::bind(&CgiTransport::processRequestBody, this, std::placeholders::_1));
	else
		write(FastCgi::Type::StdIn, id_, "", 0); // mark end-of-stream

	flush();
}

void CgiTransport::messageHeader(x0::BufferRef&& name, x0::BufferRef&& value)
{
	TRACE("CgiTransport.onResponseHeader(name:%s, value:%s)", name.str().c_str(), value.str().c_str());

	if (x0::iequals(name, "Status"))
	{
		int status = value.ref(0, value.find(' ')).toInt();
		request_->status = static_cast<x0::HttpError>(status);
	}
	else
	{
		if (name == "Location")
			request_->status = x0::HttpError::MovedTemporarily;

		request_->responseHeaders.push_back(name.str(), value.str());
	}
}

bool CgiTransport::messageContent(x0::BufferRef&& content)
{
#if X0_FASTCGI_DIRECT_IO
	TRACE("CgiTransport.messageContent(len:%ld) writeActive=%d", content.size(), writeActive_);
	if (!writeActive_)
	{
		writeActive_ = true;

		ev_io_stop(loop_, &io_);

		request_->write(
			std::make_shared<x0::BufferSource>(content),
			std::bind(&CgiTransport::writeComplete, this, std::placeholders::_1, std::placeholders::_2)
		);
	}
	else
		writeBuffer_.push_back(content);
#else
	TRACE("CgiTransport.messageContent(len:%ld)", content.size());
	writeBuffer_.push_back(content);
#endif

	return false;
}

/** \brief write-completion hook, invoked when a content chunk is written to the HTTP client.
 */
void CgiTransport::writeComplete(int err, size_t nwritten)
{
#if X0_FASTCGI_DIRECT_IO
	writeActive_ = false;

	ev_io_stop(loop_, &io_);
	ev_io_set(&io_, fd_, EV_READ);
	ev_io_start(loop_, &io_);

	if (err)
	{
		TRACE("CgiTransport.write error: %s", strerror(err));
		finish();
	}
	else if (writeBuffer_.size() != 0)
	{
		TRACE("CgiTransport.writeComplete(err=%d, nwritten=%ld), queued:%ld",
			err, nwritten, writeBuffer_.size());
		;//request_->connection.socket()->setMode(Socket::IDLE);

		request_->write(
			std::make_shared<x0::BufferSource>(std::move(writeBuffer_)),
			std::bind(&CgiTransport::writeComplete, this, std::placeholders::_1, std::placeholders::_2)
		);
		TRACE("CgiTransport.writeComplete: (after response.write call) writeBuffer_.size: %ld", writeBuffer_.size());
	}
	else if (finish_)
	{
		TRACE("CgiTransport.writeComplete(err=%d, nwritten=%ld), queue empty. finish triggered.", err, nwritten);
		finish();
	}
	else
	{
		TRACE("CgiTransport.writeComplete(err=%d, nwritten=%ld), queue empty.", err, nwritten);
		;//request_->connection.socket()->setMode(Socket::READ);
		request_->setClientAbortHandler(&CgiTransport::onClientAbort, this);
	}
#else
	TRACE("CgiTransport.writeComplete(err=%d, nwritten=%ld) %s", err, nwritten, strerror(err));
	finish();
#endif
}

/**
 * @brief invoked when remote client connected before the response has been fully transmitted.
 *
 * @param p `this pointer` to CgiTransport object
 */
void CgiTransport::onClientAbort(void *p)
{
	TRACE("CgiTransport.onClientAbort()");
	CgiTransport *self = (CgiTransport*) p;
	self->finish();
}

/**
 * \brief finishes handling the current request.
 *
 * Finishes the currently handled request, thus, making this transport connection 
 * back available for handling the next.
 */
void CgiTransport::finish()
{
	TRACE("CgiTransport::finish()");
	if (request_) {
		if (request_->status == x0::HttpError::Undefined) {
			request_->status = x0::HttpError::ServiceUnavailable;
		}

		request_->finish();
		request_ = nullptr;
	}

	context_->release(this);
}
// }}}

// {{{ CgiContext impl
uint16_t CgiContext::nextID_ = 0;

CgiContext::CgiContext(x0::HttpServer& server) :
	server_(server),
	host_(), port_(0)
{
	TRACE("CgiContext()");
}

CgiContext::~CgiContext()
{
	TRACE("~CgiContext()");
}

void CgiContext::setup(const std::string& application)
{
	size_t pos = application.find_last_of(":");

	host_ = application.substr(0, pos);
	port_ = atoi(application.substr(pos + 1).c_str());

	TRACE("CgiContext.setup(host:%s, port:%d)", host_.c_str(), port_);
}

bool CgiContext::handleRequest(x0::HttpRequest *in)
{
	TRACE("CgiContext.handleRequest()");

	CgiTransport *transport = new CgiTransport(this);

	if (!transport->open(host_.c_str(), port_)) {
		in->status = x0::HttpError::ServiceUnavailable;
		in->finish();
		return false;
	}

	if (++nextID_ == 0)
		++nextID_;

	transport->bind(in, nextID_);
	return true;
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

// {{{ fastcgi_plugin
/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class fastcgi_plugin :
	public x0::HttpPlugin
{
public:
	fastcgi_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<fastcgi_plugin, &fastcgi_plugin::handleRequest>("fastcgi");
	}

	bool handleRequest(x0::HttpRequest *in, const x0::Params& args)
	{
		if (args.count() != 1 || !args[0].isString())
			return false;

		CgiContext *cx = acquireContext(args[0].toString());
		if (!cx)
			return false;

		return cx->handleRequest(in);
	}

	CgiContext *acquireContext(const std::string& app)
	{
		auto i = contexts_.find(app);
		if (i != contexts_.end()) {
			TRACE("acquireContext('%s') available.", app.c_str());
			return i->second;
		}

		CgiContext *cx = new CgiContext(server());
		cx->setup(app);
		contexts_[app] = cx;
		TRACE("acquireContext('%s') spawned (%ld).", app.c_str(), contexts_.size());
		return cx;
	}

private:
	std::unordered_map<std::string, CgiContext *> contexts_;
}; // }}}

X0_EXPORT_PLUGIN(fastcgi)
