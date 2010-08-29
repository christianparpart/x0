/* <x0/plugins/fastcgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

/* Configuration:
 *
 *     FastCgiRemote = 'hostname:port';
 *     FastCgiMapping = { '.php', '.php4', '.php5' };
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

#if 1 // !defined(NDEBUG)
#	define TRACE(msg...) DEBUG("fastcgi: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif

class CgiContext;
class CgiTransport;

class CgiRequest : // {{{
	public x0::HttpMessageProcessor
{
private:
	int id_;

	CgiContext *context_;
	CgiTransport *transport_;

	x0::HttpRequest *request_;
	x0::HttpResponse *response_;

	FastCgi::CgiParamStreamWriter paramWriter_;

	x0::Buffer writeBuffer_;
	bool writeActive_;
	bool finish_;

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
public:
	enum State {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
	};

	CgiContext *context_;

	struct ev_loop *loop_;
	int fd_;
	State state_;

	x0::Buffer readBuffer_;
	size_t readOffset_;
	x0::Buffer writeBuffer_;
	size_t writeOffset_;
	bool flushPending_;

	std::error_code error_;

public:
	explicit CgiTransport(CgiContext *cx);
	~CgiTransport();

	State state() const { return state_; }
	std::error_code errorCode() const { return error_; }

	bool open(const char *hostname, int port);
	bool isOpen() const { return fd_ >= 0; }
	bool isClosed() const { return fd_ < 0; }
	void close();

public:
	template<typename T, typename... Args> void write(Args&&... args)
	{
		T *record = new T(args...);
		write(record);
		delete record;
	}

	void write(FastCgi::Type type, int requestId, const char *content, size_t contentLength);
	void write(FastCgi::Record *record);
	void flush();

private:
	static void io_thunk(struct ev_loop *, ev_io *w, int revents);
	inline void connected();
	inline void io(int revents);
	inline void process(const FastCgi::Record *record);
}; // }}}

class CgiContext : //{{{
	public x0::ScopeValue
{
public:
	std::string host_;
	int port_;

	CgiTransport *transport_;
	std::vector<CgiRequest *> requests_;

public:
	CgiContext();
	~CgiContext();

	void setHost(const std::string& value);
	virtual void merge(const x0::ScopeValue *cx);

	CgiRequest *handleRequest(x0::HttpRequest *in, x0::HttpResponse *out);
	void release(CgiRequest *);
	CgiRequest *request(int id) const;

public:
	x0::WriteProperty<std::string, CgiContext, &CgiContext::setHost> host;
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
	writeBuffer_(),
	writeActive_(false),
	finish_(false)
{
	TRACE("CgiRequest()");
}

CgiRequest::~CgiRequest()
{
	TRACE("~CgiRequest()");
}

int CgiRequest::id() const
{
	return id_;
}

void CgiRequest::setTransport(CgiTransport *tx)
{
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

	x0::Buffer env(paramWriter_.output());

	transport_->write(FastCgi::Type::Params, id_, env.data(), env.size());
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
	size_t np = 0;
	process(chunk, np);
}

void CgiRequest::onStdErr(x0::BufferRef&& chunk)
{
	// TODO
	//TRACE("CgiRequest.stderr: %s", chunk.str().c_str());
}

void CgiRequest::onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus)
{
	TRACE("CgiRequest.onEndRequest(appStatus=%d, protocolStatus=%d)", appStatus, (int)protocolStatus);

	if (writeActive_)
		finish_ = true;
	else
	{
		finish();
	}
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
	TRACE("CgiRequest.messageContent(len:%ld) writeActive=%d",
		content.size(), writeActive_);

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

	return false;
}

void CgiRequest::writeComplete(int err, size_t nwritten)
{
	writeActive_ = false;

	if (err)
	{
		TRACE("CgiRequest.write: %s", strerror(err));
		//finish();
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
		TRACE("CgiRequest.writeComplete(err=%d, nwritten=%ld), queue empty. finish triggered.",
			err, nwritten);
		finish();
	}
	else
	{
		TRACE("CgiRequest.writeComplete(err=%d, nwritten=%ld), queue empty.", err, nwritten);
		;//request_->connection.socket()->setMode(Socket::READ);
	}
}

void CgiRequest::finish()
{
	TRACE("CgiRequest::finish()");
	response_->finish();
	context_->release(this);
}
// }}}

// {{{ CgiTransport impl
CgiTransport::CgiTransport(CgiContext *cx) :
	context_(cx),
	loop_(ev_default_loop(0)),
	fd_(-1),
	state_(DISCONNECTED),
	readBuffer_(),
	readOffset_(0),
	writeBuffer_(),
	writeOffset_(0),
	flushPending_(false)
{
	ev_init(this, &CgiTransport::io_thunk);

	// stream management record: GetValues
#if 0
	FastCgi::CgiParamStreamWriter mr;
	mr.encode("FCGI_MPXS_CONNS", "");
	mr.encode("FCGI_MAX_CONNS", "");
	mr.encode("FCGI_MAX_REQS", "");
	x0::Buffer buf(mr.output());
	write(FastCgi::Type::GetValues, 0, buf.data(), buf.size());
#endif
}

CgiTransport::~CgiTransport()
{
	close();
}

bool CgiTransport::open(const char *hostname, int port)
{
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
				TRACE("connect: backgrounding");

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
			TRACE("connect: instant success");
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

void CgiTransport::write(FastCgi::Record *record)
{
	TRACE("CgiTransport.write(type=%s, rid=%d, size=%d, pad=%d)", 
			record->type_str(), record->requestId(), record->size(), record->paddingLength());

	writeBuffer_.push_back(record->data(), record->size());
}

void CgiTransport::flush()
{
	TRACE("CgiTransport.flush()");

	if (state_ == CONNECTED && writeBuffer_.size() != 0 && writeOffset_ == 0)
	{
		ev_io_stop(loop_, this);
		ev_io_set(this, fd_, EV_READ | EV_WRITE);
		ev_io_start(loop_, this);
	}
	else
		flushPending_ = true;
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
	{
		TRACE("connect: leaving");
		// TODO: notify requests that already wrote to us about this failure
		// for (all queued requests) { request->abort(503); }
		return;
	}

	// connect successful
	state_ = CONNECTED;

	if (writeBuffer_.size() > writeOffset_ && flushPending_)
	{
		TRACE("CgiTransport.connected() flush pending");
		flushPending_ = false;

		ev_io_stop(loop_, this);
		ev_io_set(this, fd_, EV_READ | EV_WRITE);
		ev_io_start(loop_, this);
	}
	else
	{
		TRACE("CgiTransport.connected()");
		ev_io_stop(loop_, this);
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

			int rv = ::read(fd_, const_cast<char *>(readBuffer_.data() + readBuffer_.size()), remaining);

			if (rv == 0) {
				TRACE("CgiTransport: closed.");
				close();
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
		int rv = ::write(fd_, writeBuffer_.data() + writeOffset_, writeBuffer_.size() - writeOffset_);

		if (rv < 0) {
			if (errno != EINTR && errno != EAGAIN)
				perror("CgiTransport.write");

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
			TRACE("CgiTransport: unhandled record (type:%d)", record->type());
			break;
		case FastCgi::Type::StdOut:
			context_->request(record->requestId())->onStdOut(readBuffer_.ref(record->content() - readBuffer_.data(), record->contentLength()));
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

void CgiTransport::close()
{
	TRACE("CgiTransport.close(%d)", fd_);
	if (fd_ >= 0)
	{
		ev_io_stop(loop_, this);
		::close(fd_);
		fd_ = -1;
	}
}
// }}}

// {{{ CgiContext impl
CgiContext::CgiContext() :
	host_(), port_(0),
	transport_(0),
	requests_(),
	// public properties:
	host(this)
{
}

CgiContext::~CgiContext()
{
	for (int i = 0, e = requests_.size(); i != e; ++i)
		if (requests_[i])
			release(requests_[i]);

	if (transport_) {
		delete transport_;
		transport_ = 0;
	}
}

void CgiContext::setHost(const std::string& value)
{
	size_t pos = value.find_last_of(":");

	host_ = value.substr(0, pos);
	port_ = atoi(value.substr(pos + 1).c_str());

	TRACE("setHost(host:%s, port:%d)", host_.c_str(), port_);
}

void CgiContext::merge(const x0::ScopeValue *cx)
{
}

CgiRequest *CgiContext::handleRequest(x0::HttpRequest *in, x0::HttpResponse *out)
{
	// select transport
	CgiTransport *tx = transport_;

	if (tx == 0)
		tx = new CgiTransport(this);

	if (tx->isClosed())
		tx->open(host_.c_str(), port_);

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
	request->setTransport(tx);

	return request;
}

void CgiContext::release(CgiRequest *request)
{
	request->transport()->close();
	requests_[request->id() - 1] = NULL;
	delete request;
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
private:
	CgiContext *acquire(x0::Scope& s)
	{
		CgiContext *cx = s.acquire<CgiContext>(this);
		return cx;
	}

	std::error_code setHost(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(acquire(s)->host);
	}

public:
	fastcgi_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		declareCVar("FastCgi", x0::HttpContext::host, &fastcgi_plugin::setHost);
	}

	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		CgiContext *cx = server_.resolveHost(in->hostid())->get<CgiContext>(this);
		if (!cx)
			return false;

		cx->handleRequest(in, out);

		return true;
	}
};

X0_EXPORT_PLUGIN(fastcgi);
