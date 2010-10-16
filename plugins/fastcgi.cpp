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
 *     Produces a response based on the speecified FastCGI backend.
 *     This backend is communicated with via TCP/IP.
 *     Plans to support named sockets are there, but you may
 *     jump in and contribute aswell.
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     handler fastcgi(string host_and_port); # e.g. "127.0.0.1:3000"
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
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpMessageProcessor.h>
#include <x0/io/BufferSource.h>
#include <x0/strutils.h>
#include <x0/Process.h>
#include <x0/Types.h>
#include <x0/gai_error.h>
#include <x0/StackTrace.h>
#include <x0/sysconfig.h>

#include <system_error>
#include <algorithm>
#include <string>
#include <cctype>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#if 0 // !defined(NDEBUG)
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

	std::error_code error_;

	// aka CgiRequest
	x0::HttpRequest *request_;
	x0::HttpResponse *response_;
	FastCgi::CgiParamStreamWriter paramWriter_;

#if X0_FASTCGI_DIRECT_IO
	bool writeActive_;
	bool finish_;
#endif

public:
	explicit CgiTransport(CgiContext *cx);
	~CgiTransport();

	State state() const { return state_; }
	std::error_code errorCode() const { return error_; }

	bool open(const char *hostname, int port, uint16_t id);
	bool isOpen() const { return fd_ >= 0; }
	bool isClosed() const { return fd_ < 0; }
	void close();

	void bind(x0::HttpRequest *in, x0::HttpResponse *out);

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

	void write(FastCgi::Type type, int requestId, const char *content, size_t contentLength);
	void write(FastCgi::Type type, int requestId, x0::Buffer&& content);
	void write(FastCgi::Record *record);
	void flush();

private:
	void processRequestBody(x0::BufferRef&& chunk);

	virtual void messageHeader(x0::BufferRef&& name, x0::BufferRef&& value);
	virtual bool messageContent(x0::BufferRef&& content);
	void writeComplete(int error, size_t nwritten);

	void finish();

	static void io_thunk(struct ev_loop *, ev_io *w, int revents);

	inline void connected();
	inline void io(int revents);
	inline void processRecord(const FastCgi::Record *record);
	inline void onParam(const std::string& name, const std::string& value);
}; // }}}

uint16_t nextID_ = 0;
class CgiContext //{{{
{
public:
	x0::HttpServer& server_;
	std::string host_;
	int port_;

	CgiTransport *transport_;
	std::vector<CgiTransport *> transports_;

public:
	CgiContext(x0::HttpServer& server);
	~CgiContext();

	x0::HttpServer& server() const { return server_; }
	void setup(const std::string& application);

	CgiTransport *handleRequest(x0::HttpRequest *in, x0::HttpResponse *out);

	void release(CgiTransport *tr);
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

	request_(NULL),
	response_(NULL),
	paramWriter_()
#if X0_FASTCGI_DIRECT_IO
	, writeActive_(false)
	, finish_(false)
#endif
{
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
	close();
}

bool CgiTransport::open(const char *hostname, int port, uint16_t id)
{
	hostname_ = hostname;
	port_ = port;
	id_ = id;

	TRACE("connect(hostname=%s, port=%d)", hostname, port);

	struct addrinfo hints;
	struct addrinfo *res;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char sport[16];
	snprintf(sport, sizeof(sport), "%d", port);

	int rv = getaddrinfo(hostname, sport, &hints, &res);
	if (rv)
	{
		error_ = make_error_code(static_cast<enum x0::gai_error>(rv));
		context().server().log(x0::Severity::error,
				"fastcgi: could not get addrinfo of %s:%s: %s", hostname, sport, error_.message().c_str());
		return false;
	}

	for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next)
	{
		fd_ = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd_ < 0)
		{
			error_ = std::make_error_code(static_cast<std::errc>(errno));
			continue;
		}

		fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK | O_CLOEXEC);

		rv = ::connect(fd_, rp->ai_addr, rp->ai_addrlen);
		if (rv < 0)
		{
			if (errno == EINPROGRESS)
			{
				TRACE("connect: backgrounding (fd:%d)", fd_);

				state_ = CONNECTING;
				ev_io_set(&io_, fd_, EV_WRITE);
				ev_io_start(loop_, &io_);

				break;
			}
			else
			{
				error_ = std::make_error_code(static_cast<std::errc>(errno));
				context().server().log(x0::Severity::error,
					"fastcgi: could not connect to %s:%s: %s", hostname_.c_str(), sport, strerror(errno));
				close();
			}
		}
		else
		{
			TRACE("connect: instant success (fd:%d)", fd_);
			connected();
			break;
		}
	}

	freeaddrinfo(res);

	return fd_ >= 0;
}

void CgiTransport::bind(x0::HttpRequest *in, x0::HttpResponse *out)
{
	request_ = in;
	response_ = out;

	beginRequest();
	streamParams();

	flush();
}

void CgiTransport::write(FastCgi::Type type, int requestId, const char *content, size_t contentLength)
{
	FastCgi::Record *record = FastCgi::Record::create(type, requestId, contentLength);
	memcpy(const_cast<char *>(record->content()), content, contentLength);
	write(record);
	delete record;
}

void CgiTransport::write(FastCgi::Type type, int requestId, x0::Buffer&& content)
{
#if 1
	FastCgi::Record *record = FastCgi::Record::create(type, requestId, content.size());
	memcpy(const_cast<char *>(record->content()), content.data(), content.size());
	write(record);
	delete record;
#else
	FastCgi::Record record(type, requestId, content.size(), 0);
	writeBuffer_.push_back(record.data(), record.size());
	writeBuffer_.push_back(content.data(), content.size());

	TRACE("CgiTransport.write(type=%s, rid=%d, size=%d, pad=%d)", 
			record.type_str(), record.requestId(), record.size(), record.paddingLength());
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
		cc->connected();
	else
		cc->io(revents);
}

void CgiTransport::connected()
{
	int val = 0;
	socklen_t vlen = sizeof(val);
	if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0)
	{
		if (val == 0)
		{
			TRACE("connect: success");
		}
		else
		{
			error_ = std::make_error_code(static_cast<std::errc>(val));
			context().server().log(x0::Severity::error,
				"fastcgi: could not connect to %s:%d: %s", hostname_.c_str(), port_, strerror(val));
			close();
		}
	}
	else
	{
		error_ = std::make_error_code(static_cast<std::errc>(errno));
		TRACE("connect: getsocketopt() error: %s", error_.message().c_str());
		context().server().log(x0::Severity::error,
			"fastcgi: could not getsocketopt(SO_ERROR) to %s:%d: %s", hostname_.c_str(), port_, strerror(errno));
		close();
	}

	if (!isOpen())
		return;

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
}

void CgiTransport::io(int revents)
{
	if (revents & EV_READ)
	{
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
				close();
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
		while (readOffset_ + sizeof(FastCgi::Record) < readBuffer_.size())
		{
			const FastCgi::Record *record =
				reinterpret_cast<const FastCgi::Record *>(readBuffer_.data() + readOffset_);

			// payload fully available?
			if (readBuffer_.size() - readOffset_ < record->size())
				break;

			readOffset_ += record->size();

			processRecord(record);
		}
	}

	if (revents & EV_WRITE)
	{
		//size_t sz = writeBuffer_.size() - writeOffset_;
		ssize_t rv = ::write(fd_, writeBuffer_.data() + writeOffset_, writeBuffer_.size() - writeOffset_);
		//TRACE("CgiTransport.write(fd:%d): %ld -> %ld\n", fd_, sz, rv);

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
			ev_io_stop(loop_, &io_);
			ev_io_set(&io_, fd_, EV_READ);
			ev_io_start(loop_, &io_);

			writeBuffer_.clear();
			writeOffset_ = 0;
		}
	}
}

void CgiTransport::processRecord(const FastCgi::Record *record)
{
	TRACE("processRecord(type=%s (%d), rid=%d, contentLength=%d, paddingLength=%d)",
		record->type_str(), record->type(), record->requestId(),
		record->contentLength(), record->paddingLength());

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
			break;
		case FastCgi::Type::UnknownType:
		default:
			context().server().log(x0::Severity::error,
				"fastcgi: unknown transport record received from backend %s:%d. type:%d, payload-size:%ld",
				hostname_.c_str(), port_, record->type(), record->contentLength());
			break;
	}
}

void CgiTransport::onParam(const std::string& name, const std::string& value)
{
	TRACE("onParam(%s, %s)", name.c_str(), value.c_str());
}

void CgiTransport::close()
{
	TRACE("CgiTransport.close(%d)", fd_);

	if (response_)
		finish();

	if (fd_ >= 0)
	{
		ev_io_stop(loop_, &io_);
		::close(fd_);
		fd_ = -1;
	}
}

void CgiTransport::beginRequest()
{
	write<FastCgi::BeginRequestRecord>(FastCgi::Role::Responder, id_, true);
}

void CgiTransport::streamParams()
{
	paramWriter_.encode("SERVER_SOFTWARE", PACKAGE_NAME "/" PACKAGE_VERSION);
	paramWriter_.encode("SERVER_NAME", request_->header("Host"));
	paramWriter_.encode("GATEWAY_INTERFACE", "CGI/1.1");

	paramWriter_.encode("SERVER_PROTOCOL", "1.1");
	paramWriter_.encode("SERVER_ADDR", request_->connection.localIP());
	paramWriter_.encode("SERVER_PORT", boost::lexical_cast<std::string>(request_->connection.localPort()));// TODO this should to be itoa'd only ONCE

	paramWriter_.encode("REQUEST_METHOD", request_->method);

	request_->updatePathInfo(); // should we invoke this explicitely? I'd vote for no... however.

	paramWriter_.encode("SCRIPT_NAME", request_->path);
	paramWriter_.encode("PATH_INFO", request_->pathinfo);
	if (!request_->pathinfo.empty())
		paramWriter_.encode("PATH_TRANSLATED", request_->document_root, request_->pathinfo);

	paramWriter_.encode("QUERY_STRING", request_->query);			// unparsed uri
	paramWriter_.encode("REQUEST_URI", request_->uri);

	//paramWriter_.encode("REMOTE_HOST", "");  // optional
	paramWriter_.encode("REMOTE_ADDR", request_->connection.remoteIP());
	paramWriter_.encode("REMOTE_PORT", boost::lexical_cast<std::string>(request_->connection.remotePort()));

	//paramWriter_.encode("AUTH_TYPE", ""); // TODO
	//paramWriter_.encode("REMOTE_USER", "");
	//paramWriter_.encode("REMOTE_IDENT", "");

	if (request_->content_available()) {
		paramWriter_.encode("CONTENT_TYPE", request_->header("Content-Type"));
		paramWriter_.encode("CONTENT_LENGTH", request_->header("Content-Length"));

		request_->read(std::bind(&CgiTransport::processRequestBody, this, std::placeholders::_1));
	}

#if defined(WITH_SSL)
	if (request_->connection.isSecure())
		paramWriter_.encode("HTTPS", "1");
#endif

	// HTTP request headers
	for (auto i = request_->headers.begin(), e = request_->headers.end(); i != e; ++i)
	{
		std::string key;
		key.reserve(5 + i->name.size());
		key += "HTTP_";

		for (auto p = i->name.begin(), q = i->name.end(); p != q; ++p)
			key += std::isalnum(*p) ? std::toupper(*p) : '_';

		paramWriter_.encode(key, i->value);
	}
	paramWriter_.encode("DOCUMENT_ROOT", request_->document_root);
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

	request_->connection.server().log(x0::Severity::error, "fastcgi: %s", chomp(chunk.str()).c_str());
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
		response_->write(
			std::make_shared<x0::BufferSource>(writeBuffer_),
			std::bind(&CgiTransport::writeComplete, this, std::placeholders::_1, std::placeholders::_2)
		);
	else
		finish();
#endif
}

void CgiTransport::processRequestBody(x0::BufferRef&& chunk)
{
	TRACE("CgiTransport.processRequestBody(len=%ld)", chunk.size());
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
		response_->status = static_cast<x0::HttpError>(value.toInt());
		TRACE("CgiTransport.status := %s", response_->status_str(response_->status).c_str());
	}
	else
		response_->headers.push_back(name.str(), value.str());
}

bool CgiTransport::messageContent(x0::BufferRef&& content)
{
#if X0_FASTCGI_DIRECT_IO
	TRACE("CgiTransport.messageContent(len:%ld) writeActive=%d", content.size(), writeActive_);
	if (!writeActive_)
	{
		writeActive_ = true;

		ev_io_stop(loop_, &io_);

		response_->write(
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

		response_->write(
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
	}
#else
	TRACE("CgiTransport.writeComplete(err=%d, nwritten=%ld) %s", err, nwritten, strerror(err));
	finish();
#endif
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

	if (!response_)
		return;

	if (response_->status == x0::HttpError::Undefined)
	{
		response_->status = x0::HttpError::ServiceUnavailable;
		//x0::StackTrace st;
		//printf("%s\n", st.c_str());
	}

	response_->finish();

	request_ = NULL;
	response_ = NULL;

	context_->release(this);
}
// }}}

// {{{ CgiContext impl
CgiContext::CgiContext(x0::HttpServer& server) :
	server_(server),
	host_(), port_(0),
	transport_(0),
	transports_()
{
}

CgiContext::~CgiContext()
{
	TRACE("~CgiContext()");

//	for (int i = 0, e = transports_.size(); i != e; ++i)
//		if (transports_[i])
//			release(transports_[i]);

	if (transport_) {
		delete transport_;
		transport_ = 0;
	}
}

void CgiContext::setup(const std::string& application)
{
	size_t pos = application.find_last_of(":");

	host_ = application.substr(0, pos);
	port_ = atoi(application.substr(pos + 1).c_str());

	TRACE("CgiContext.setup(host:%s, port:%d)", host_.c_str(), port_);
}

CgiTransport *CgiContext::handleRequest(x0::HttpRequest *in, x0::HttpResponse *out)
{
	TRACE("CgiContext.handleRequest()");

	// select transport
	CgiTransport *transport = transport_; // TODO support more than one transport and LB between them

	if (++nextID_ == 0)
		++nextID_;

	if (transport == 0)
	{
		transport = new CgiTransport(this);
		transport->open(host_.c_str(), port_, nextID_);
	}
	else if (transport->isClosed())
		transport->open(host_.c_str(), port_, nextID_);


	// attach request to transport
	transport->bind(in, out);

	return transport;
}

/**
 * \brief enqueues this transport connection ready for serving the next request.
 * \param transport the transport connection object
 */
void CgiContext::release(CgiTransport *transport)
{
	TRACE("CgiContext.release()");
	// TODO enqueue instead of destroying.
	delete transport;
}
//}}}

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

	bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		if (args.count() != 1 || !args[0].isString())
			return false;

		CgiContext *cx = acquireContext(args[0].toString());
		if (!cx)
			return false;

		return cx->handleRequest(in, out);
	}

	CgiContext *acquireContext(const std::string& app)
	{
		CgiContext *cx = new CgiContext(server());
		cx->setup(app);
		return cx;
	}
};

X0_EXPORT_PLUGIN(fastcgi)
