/* <x0/plugins/fastcgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

/* Configuration:
 *
 *     action handler;
 *         fastcgi IP, PORT
 *         fastcgi unix_socket
 *         fastcgi ID
 *
 *     setup functions:
 *         fastcgi.register ID, IP, PORTs
 *         fastcgi.register ID, UNIX_SOCKET
 *
 * Todo:
 *
 *   - error handling, including:
 *       - early http client abort (raises EndRequestRecord-submission to application)
 *       - stdout stream parse errors,
 *       - transport level errors (connect/read/write errors)
 *       - timeouts
 *   - query management cvars from each fastcgi application and honor them
 *   - load balance across multiple remotes
 *   - failover to another transport until one accepts the request
 *     or the deliver-timeout is exceeded, which results into an error 503 (service unavailable)
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
#	include <x0/StackTrace.h>
#	define TRACE(msg...) DEBUG("fastcgi: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif

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

class CgiRequest : // {{{
	public x0::HttpMessageProcessor
{
	friend class CgiTransport;

private:
	int id_;

	CgiContext *context_;
	CgiTransport *transport_;

	x0::HttpRequest *request_;
	x0::HttpResponse *response_;

	FastCgi::CgiParamStreamWriter paramWriter_;

	x0::Buffer writeBuffer_;

#if X0_FASTCGI_DIRECT_IO
	bool writeActive_;
	bool finish_;
#endif

public:
	CgiRequest(CgiContext *cx, int id, x0::HttpRequest *in, x0::HttpResponse *out);
	~CgiRequest();

	inline int id() const;

	void setTransport(CgiTransport *);
	inline CgiTransport *transport() const;

	// server-to-application
	void beginRequest();
	void streamParams();
	void streamStdIn();
	void streamData();
	void abortRequest();

	// application-to-server
	void onStdOut(x0::BufferRef&& chunk);
	void onStdErr(x0::BufferRef&& chunk);
	void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

private:
	void processRequestBody(x0::BufferRef&& chunk);

	virtual void messageHeader(x0::BufferRef&& name, x0::BufferRef&& value);
	virtual bool messageContent(x0::BufferRef&& content);
	void writeComplete(int error, size_t nwritten);

	void finish();
}; // }}}

class CgiTransport : // {{{
	public ev_io
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
		CONNECTED,
	};

	CgiContext *context_;

	struct ev_loop *loop_;

	std::string hostname_;
	int port_;

	int fd_;
	State state_;

	std::list<CgiRequest *> requests_;

	x0::Buffer readBuffer_;
	size_t readOffset_;
	x0::Buffer writeBuffer_;
	size_t writeOffset_;
	bool flushPending_;

	bool canMultiplex_;  // can multiplex connections,requests
	int maxConnections_; // max. number of concurrent connections
	int maxRequests_;    // max. number of concurrent requests per connection
	bool configured_;

	std::error_code error_;

public:
	explicit CgiTransport(CgiContext *cx);
	~CgiTransport();

	State state() const { return state_; }
	std::error_code errorCode() const { return error_; }

	bool open(const char *hostname, int port);
	void reopen();
	bool isOpen() const { return fd_ >= 0; }
	bool isClosed() const { return fd_ < 0; }
	void close();

	void push(CgiRequest *request);
	void release(CgiRequest *request);

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
	static void io_thunk(struct ev_loop *, ev_io *w, int revents);
	inline void connected();
	inline void io(int revents);
	inline void process(const FastCgi::Record *record);
	inline void onParam(const std::string& name, const std::string& value);
}; // }}}

class CgiContext //{{{
{
public:
	std::string host_;
	int port_;

	CgiTransport *transport_;
	std::vector<CgiRequest *> requests_;

public:
	CgiContext();
	~CgiContext();

	void setup(const std::string& value);

	CgiRequest *handleRequest(x0::HttpRequest *in, x0::HttpResponse *out);
	void release(CgiRequest *);
	CgiRequest *request(int id) const;
};
// }}}

// {{{ CgiRequest impl
CgiRequest::CgiRequest(CgiContext *cx, int id, x0::HttpRequest *in, x0::HttpResponse *out) :
	HttpMessageProcessor(x0::HttpMessageProcessor::MESSAGE),
	id_(id),
	context_(cx),
	transport_(NULL),
	request_(in),
	response_(out),
	paramWriter_(),
	writeBuffer_()
#if X0_FASTCGI_DIRECT_IO
	, writeActive_(false)
	, finish_(false)
#endif
{
	TRACE("CgiRequest()");
}

CgiRequest::~CgiRequest()
{
	TRACE("~CgiRequest()");
//	x0::StackTrace st;
//	TRACE("Stack Trace:\n%s\n", st.c_str());
}

int CgiRequest::id() const
{
	return id_;
}

void CgiRequest::setTransport(CgiTransport *tx)
{
	TRACE("CgiRequest.setTransport(%p)", tx);
	transport_ = tx;

	if (transport_)
	{
		beginRequest();
		streamParams();

		transport_->flush();
	}
}

CgiTransport *CgiRequest::transport() const
{
	return transport_;
}

void CgiRequest::beginRequest()
{
	transport_->write<FastCgi::BeginRequestRecord>(FastCgi::Role::Responder, id_, true);
}

void CgiRequest::streamParams()
{
	paramWriter_.encode("SERVER_SOFTWARE", PACKAGE_NAME "/" PACKAGE_VERSION);
	paramWriter_.encode("SERVER_NAME", request_->header("Host"));
	paramWriter_.encode("GATEWAY_INTERFACE", "CGI/1.1");

	paramWriter_.encode("SERVER_PROTOCOL", "1.1");
	paramWriter_.encode("SERVER_ADDR", "localhost"); // TODO
	paramWriter_.encode("SERVER_PORT", "8080"); // TODO

	paramWriter_.encode("REQUEST_METHOD", request_->method);
	paramWriter_.encode("PATH_INFO", request_->path);
	paramWriter_.encode("PATH_TRANSLATED", request_->fileinfo->filename());
	paramWriter_.encode("SCRIPT_NAME", request_->path);
	paramWriter_.encode("QUERY_STRING", request_->query);			// unparsed uri
	paramWriter_.encode("REQUEST_URI", request_->uri);

	//paramWriter_.encode("REMOTE_HOST", "");  // optional
	paramWriter_.encode("REMOTE_ADDR", request_->connection.remote_ip());
	paramWriter_.encode("REMOTE_PORT", boost::lexical_cast<std::string>(request_->connection.remote_port()));

	//paramWriter_.encode("AUTH_TYPE", ""); // TODO
	//paramWriter_.encode("REMOTE_USER", "");
	//paramWriter_.encode("REMOTE_IDENT", "");

	if (request_->content_available()) {
		paramWriter_.encode("CONTENT_TYPE", request_->header("Content-Type"));
		paramWriter_.encode("CONTENT_LENGTH", request_->header("Content-Length"));

		request_->read(std::bind(&CgiRequest::processRequestBody, this, std::placeholders::_1));
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

	transport_->write(FastCgi::Type::Params, id_, paramWriter_.output());
	transport_->write(FastCgi::Type::Params, id_, "", 0); // EOS
}

void CgiRequest::streamStdIn()
{
	// TODO
}

void CgiRequest::streamData()
{
}

void CgiRequest::abortRequest()
{
	transport_->write<FastCgi::AbortRequestRecord>(id_);
	transport_->flush();
}

void CgiRequest::onStdOut(x0::BufferRef&& chunk)
{
	TRACE("CgiRequest.onStdOut(chunk.size:%ld)", chunk.size());
	size_t np = 0;
	process(chunk, np);
}

void CgiRequest::onStdErr(x0::BufferRef&& chunk)
{
	TRACE("CgiRequest.stderr: %s", chomp(chunk.str()).c_str());
	// TODO
}

void CgiRequest::onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus)
{
	TRACE("CgiRequest.onEndRequest(appStatus=%d, protocolStatus=%d)", appStatus, (int)protocolStatus);

#if X0_FASTCGI_DIRECT_IO
	if (writeActive_)
		finish_ = true;
	else
		finish();
#else
	if (writeBuffer_.size() != 0)
		response_->write(
			std::make_shared<x0::BufferSource>(writeBuffer_),
			std::bind(&CgiRequest::writeComplete, this, std::placeholders::_1, std::placeholders::_2)
		);
	else
		finish();
#endif
}

void CgiRequest::processRequestBody(x0::BufferRef&& chunk)
{
	transport_->write(FastCgi::Type::StdIn, id_, chunk.data(), chunk.size());
}

void CgiRequest::messageHeader(x0::BufferRef&& name, x0::BufferRef&& value)
{
	TRACE("CgiRequest.onResponseHeader(name:%s, value:%s)", name.str().c_str(), value.str().c_str());

	if (x0::iequals(name, "Status"))
	{
		response_->status = static_cast<x0::http_error>(value.toInt());
		TRACE("CgiRequest.status := %s", response_->status_str(response_->status).c_str());
	}
	else
		response_->headers.set(name.str(), value.str());
}

bool CgiRequest::messageContent(x0::BufferRef&& content)
{
#if X0_FASTCGI_DIRECT_IO
	TRACE("CgiRequest.messageContent(len:%ld) writeActive=%d", content.size(), writeActive_);
	if (!writeActive_)
	{
		writeActive_ = true;
		response_->write(
			std::make_shared<x0::BufferSource>(content),
			std::bind(&CgiRequest::writeComplete, this, std::placeholders::_1, std::placeholders::_2)
		);
	}
	else
		writeBuffer_.push_back(content);
#else
	TRACE("CgiRequest.messageContent(len:%ld)", content.size());
	writeBuffer_.push_back(content);
#endif

	return false;
}

void CgiRequest::writeComplete(int err, size_t nwritten)
{
#if X0_FASTCGI_DIRECT_IO
	writeActive_ = false;

	if (err)
	{
		TRACE("CgiRequest.write error: %s", strerror(err));
		finish();
	}
	else if (writeBuffer_.size() != 0)
	{
		TRACE("CgiRequest.writeComplete(err=%d, nwritten=%ld), queued:%ld",
			err, nwritten, writeBuffer_.size());
		;//request_->connection.socket()->setMode(Socket::IDLE);

		response_->write(
			std::make_shared<x0::BufferSource>(std::move(writeBuffer_)),
			std::bind(&CgiRequest::writeComplete, this, std::placeholders::_1, std::placeholders::_2)
		);
	}
	else if (finish_)
	{
		TRACE("CgiRequest.writeComplete(err=%d, nwritten=%ld), queue empty. finish triggered.", err, nwritten);
		finish();
	}
	else
	{
		TRACE("CgiRequest.writeComplete(err=%d, nwritten=%ld), queue empty.", err, nwritten);
		;//request_->connection.socket()->setMode(Socket::READ);
	}
#else
	TRACE("CgiRequest.writeComplete(err=%d, nwritten=%ld) %s", err, nwritten, strerror(err));
	finish();
#endif
}

void CgiRequest::finish()
{
	TRACE("CgiRequest::finish()");

	if (response_->status == x0::http_error::undefined)
		response_->status = x0::http_error::service_unavailable;

	response_->finish();
	context_->release(this);
}
// }}}

// {{{ CgiTransport impl
CgiTransport::CgiTransport(CgiContext *cx) :
	context_(cx),
	loop_(ev_default_loop(0)),
	hostname_(),
	port_(0),
	fd_(-1),
	state_(DISCONNECTED),
	readBuffer_(),
	readOffset_(0),
	writeBuffer_(),
	writeOffset_(0),
	flushPending_(false),
	canMultiplex_(false),
	maxConnections_(1),
	maxRequests_(1),
	configured_(false)
{
	ev_init(this, &CgiTransport::io_thunk);

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

bool CgiTransport::open(const char *hostname, int port)
{
	hostname_ = hostname;
	port_ = port;

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
		TRACE("connect: resolve error: %s", error_.message().c_str());
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
				ev_io_set(this, fd_, EV_WRITE);
				ev_io_start(loop_, this);

				break;
			}
			else
			{
				error_ = std::make_error_code(static_cast<std::errc>(errno));
				TRACE("connect error: %s (category: %s)", error_.message().c_str(), error_.category().name());
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
		ev_io_stop(loop_, this);
		ev_io_set(this, fd_, EV_READ | EV_WRITE);
		ev_io_start(loop_, this);
	}
	else
	{
		TRACE("CgiTransport.flush() -> pending");
		flushPending_ = true;
	}
}

void CgiTransport::io_thunk(struct ev_loop *, ev_io *w, int revents)
{
	CgiTransport *cc = reinterpret_cast<CgiTransport *>(w);

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
			TRACE("connect: error(%d): %s", val, error_.message().c_str());
			close();
		}
	}
	else
	{
		error_ = std::make_error_code(static_cast<std::errc>(errno));
		TRACE("connect: getsocketopt() error: %s", error_.message().c_str());
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

		ev_io_set(this, fd_, EV_READ | EV_WRITE);
		ev_io_start(loop_, this);
	}
	else
	{
		TRACE("CgiTransport.connected()");
		ev_io_set(this, fd_, EV_READ);
		ev_io_start(loop_, this);
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
				TRACE("fcgiapp closed connection");
				reopen();
				return;
			}

			if (rv < 0) {
				if (errno != EINTR && errno != EAGAIN)
					perror("CgiTransport.read");

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

			process(record);
		}
	}

	if (revents & EV_WRITE)
	{
		size_t sz = writeBuffer_.size() - writeOffset_;
		ssize_t rv = ::write(fd_, writeBuffer_.data() + writeOffset_, writeBuffer_.size() - writeOffset_);
		printf("CgiTransport.write(fd:%d): %ld -> %ld\n", fd_, sz, rv);

		if (rv < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				printf("CgiTransport.write(fd:%d): %s\n", fd_, strerror(errno));
			}

			return;
		}

		writeOffset_ += rv;

		if (writeOffset_ == writeBuffer_.size()) {
			ev_io_stop(loop_, this);
			ev_io_set(this, fd_, EV_READ);
			ev_io_start(loop_, this);

			writeBuffer_.clear();
			writeOffset_ = 0;
		}
	}
}

void CgiTransport::process(const FastCgi::Record *record)
{
	TRACE("process(type=%s (%d), rid=%d, contentLength=%d, paddingLength=%d)",
		record->type_str(), record->type(), record->requestId(),
		record->contentLength(), record->paddingLength());

	switch (record->type())
	{
		case FastCgi::Type::GetValuesResult:
			ParamReader(this).processParams(record->content(), record->contentLength());
			configured_ = true; // should be set *only* at EOS of GetValuesResult? we currently guess, that there'll be only *one* packet
			break;
		case FastCgi::Type::StdOut:
			context_->request(record->requestId())->onStdOut(
					readBuffer_.ref(record->content() - readBuffer_.data(), record->contentLength()));
			break;
		case FastCgi::Type::StdErr:
			context_->request(record->requestId())->onStdErr(readBuffer_.ref(record->content() - readBuffer_.data(), record->contentLength()));
			break;
		case FastCgi::Type::EndRequest:
			context_->request(record->requestId())->onEndRequest(
					static_cast<const FastCgi::EndRequestRecord *>(record)->appStatus(),
					static_cast<const FastCgi::EndRequestRecord *>(record)->protocolStatus()
			);
			break;
		case FastCgi::Type::UnknownType:
		default:
			TRACE("CgiTransport: unhandled record (type:%d)", record->type());
			break;
	}
}

void CgiTransport::onParam(const std::string& name, const std::string& value)
{
	TRACE("onParam(%s, %s)", name.c_str(), value.c_str());

	if (name == "FCGI_MPXS_CONNS")
		canMultiplex_ = std::atoi(value.c_str()) != 0;
	else if (name == "FCGI_MAX_CONNS")
		maxConnections_ = std::atoi(value.c_str());
	else if (name == "FCGI_MAX_REQS")
		maxRequests_ = std::atoi(value.c_str());
	else
		TRACE("warning: unknown management parameter: '%s'", name.c_str());
}

void CgiTransport::reopen()
{
	TRACE("CgiTransport.reopen()");
	if (fd_ >= 0)
	{
		ev_io_stop(loop_, this);
		::close(fd_);
		fd_ = -1;
	}

	open(hostname_.c_str(), port_);
}

void CgiTransport::close()
{
	TRACE("CgiTransport.close(%d)", fd_);

	if (fd_ >= 0)
	{
		ev_io_stop(loop_, this);
		::close(fd_);
		fd_ = -1;
	}

	while (!requests_.empty())
		requests_.front()->finish();
}

void CgiTransport::push(CgiRequest *request)
{
	requests_.push_back(request);
	request->setTransport(this);
}

void CgiTransport::release(CgiRequest *request)
{
	for (auto i = requests_.begin(), e = requests_.end(); i != e; ++i) {
		if (*i == request) {
			requests_.erase(i);
			request->setTransport(NULL);
			return;
		}
	}
}
// }}}

// {{{ CgiContext impl
CgiContext::CgiContext() :
	host_(), port_(0),
	transport_(0),
	requests_()
{
}

CgiContext::~CgiContext()
{
	TRACE("~CgiContext()");

	for (int i = 0, e = requests_.size(); i != e; ++i)
		if (requests_[i])
			release(requests_[i]);

	if (transport_) {
		delete transport_;
		transport_ = 0;
	}
}

void CgiContext::setup(const std::string& value)
{
	size_t pos = value.find_last_of(":");

	host_ = value.substr(0, pos);
	port_ = atoi(value.substr(pos + 1).c_str());

	TRACE("CgiContext.setup(host:%s, port:%d)", host_.c_str(), port_);
}

CgiRequest *CgiContext::handleRequest(x0::HttpRequest *in, x0::HttpResponse *out)
{
	TRACE("CgiContext.handleRequest()");
	// select transport
	CgiTransport *transport = transport_; // TODO support more than one transport and LB between them

	if (transport == 0)
	{
		transport = new CgiTransport(this);
		transport->open(host_.c_str(), port_);
	}
	else if (transport->isClosed())
		transport->reopen();

	// allocate request ID
	int i = 0;
	int e = requests_.size();

	while (i != e)
		if (requests_[i] != NULL)
			break;
		else
			++i;

	if (i == e)
		requests_.push_back(NULL);

	// create request
	CgiRequest *request = new CgiRequest(this, i + 1, in, out);
	requests_[i] = request;

	// attach request to transport
	transport->push(request);

	return request;
}

void CgiContext::release(CgiRequest *request)
{
	CgiTransport *transport = request->transport();

	if (transport)
		transport->release(request);

	requests_[request->id() - 1] = NULL;

	delete request;

	if (transport)
		transport->close(); // TODO: keep-alive (php doesn't like it?)
}

CgiRequest *CgiContext::request(int id) const
{
	return requests_[id - 1];
}
//}}}

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class fastcgi_plugin :
	public x0::HttpPlugin,
	public x0::IHttpRequestHandler
{
public:
	fastcgi_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
	}

	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
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
		CgiContext *cx = new CgiContext();
		cx->setup(app);
		return cx;
	}
};

X0_EXPORT_PLUGIN(fastcgi);
