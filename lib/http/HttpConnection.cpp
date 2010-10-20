/* <x0/HttpConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpConnection.h>
#include <x0/http/HttpListener.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpServer.h>
#include <x0/SocketDriver.h>
#include <x0/StackTrace.h>
#include <x0/Socket.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <functional>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#if 0 // !defined(NDEBUG)
#	define TRACE(msg...) DEBUG("HttpConnection: " msg)
#else
#	define TRACE(msg...)
#endif

#define X0_HTTP_STRICT 1

namespace x0 {

#if !defined(NDEBUG) // {{{ struct ConnectionLogger
struct ConnectionLogger :
	public CustomData
{
private:
	static unsigned connection_counter;

	HttpServer& server_;
	ev::tstamp start_;
	unsigned cid_;
	unsigned rcount_;
	FILE *fp;

private:
	template<typename... Args>
	void log(Severity s, const char *fmt, Args&& ... args)
	{
		server_.log(s, fmt, args...);

		if (fp)
		{
			fprintf(fp, "%.4f ", ev_now(server_.loop()));
			fprintf(fp, fmt, args...);
			fprintf(fp, "\n");
			fflush(fp);
		}
	}

public:
	explicit ConnectionLogger(HttpServer& server) :
		server_(server),
		start_(ev_now(server.loop())),
		cid_(++connection_counter),
		rcount_(0),
		fp(NULL)
	{

		char buf[1024];
		snprintf(buf, sizeof(buf), "c-io-%04d.log", id());
		fp = fopen(buf, "w");

		log(Severity::info, "HttpConnection[%d] opened.", id());
	}

	ev::tstamp connection_time() const { return ev_now(server_.loop()) - start_; }
	unsigned id() const { return cid_; }
	unsigned request_count() const { return rcount_; }

	void log(const BufferRef& buf)
	{
		fprintf(fp, "%.4f %ld\r\n", ev_now(server_.loop()), buf.size());
		fwrite(buf.data(), buf.size(), 1, fp);
		fprintf(fp, "\r\n");
		fflush(fp);
	}

	~ConnectionLogger()
	{
		log(Severity::info, "HttpConnection[%d] closed. timing: %.4f (nreqs: %d)",
				id(), connection_time(), request_count());

		if (fp)
			fclose(fp);
	}
};

unsigned ConnectionLogger::connection_counter = 0;
#endif
// }}}

/**
 * \class HttpConnection
 * \brief represents an HTTP connection handling incoming requests.
 *
 * The \p HttpConnection object is to be allocated once an HTTP client connects
 * to the HTTP server and was accepted by the \p HttpListener.
 * It will also own the respective request and response objects created to serve
 * the requests passed through this connection.
 */

/** initializes a new connection object, created by given listener.
 * \param lst the listener object that created this connection.
 * \note This triggers the onConnectionOpen event.
 */
HttpConnection::HttpConnection(HttpListener& lst, HttpWorker& w, int fd) :
	HttpMessageProcessor(HttpMessageProcessor::REQUEST),
	secure(false),
	listener_(lst),
	worker_(w),
	server_(lst.server()),
	socket_(0),
	active_(true), // when this is constricuted, it *must* be active right now :) 
	buffer_(8192),
	offset_(0),
	request_count_(0),
	request_(0),
	response_(0),
	source_(),
	sink_(NULL),
	onWriteComplete_(),
	bytesTransferred_(0)
#if !defined(NDEBUG)
	, ctime_(ev_now(loop()))
#endif
{
	socket_ = listener_.socketDriver()->create(fd, loop());
	sink_.setSocket(socket_);

	TRACE("(%p): fd=%d", this, socket_->handle());

#if defined(TCP_NODELAY)
	if (server_.tcp_nodelay())
		socket_->setTcpNoDelay(true);
#endif

#if !defined(NDEBUG)
	//custom_data[(HttpPlugin *)this] = std::make_shared<ConnectionLogger>(server_);
#endif

	server_.onConnectionOpen(this);
}

/** releases all connection resources  and triggers the onConnectionClose event.
 */
HttpConnection::~HttpConnection()
{
	delete request_;
	delete response_;
	request_ = 0;
	response_ = 0;

	TRACE("~(%p)", this);
	//TRACE("Stack Trace:\n%s", StackTrace().c_str());

	try
	{
		server_.onConnectionClose(this); // we cannot pass a shared pointer here as use_count is already zero and it would just lead into an exception though
	}
	catch (...)
	{
		TRACE("~HttpConnection(%p): unexpected exception", this);
	}

	if (socket_)
	{
		delete socket_;
#if !defined(NDEBUG)
		socket_ = 0;
#endif
	}
}

void HttpConnection::io(Socket *)
{
	TRACE("(%p).io(mode=%s)", this, socket_->mode_str());
	active_ = true;

	switch (socket_->mode())
	{
		case Socket::READ:
			processInput();
			break;
		case Socket::WRITE:
			processOutput();
			break;
		default:
			break;
	}

	if (isClosed())
		delete this;
	else
		active_ = false;
}

void HttpConnection::timeout(Socket *)
{
	TRACE("(%p): timed out", this);

//	ev_unloop(loop(), EVUNLOOP_ONE);

	delete this;
}

#if defined(WITH_SSL)
bool HttpConnection::isSecure() const
{
	return listener_.isSecure();
}
#endif

/** start first async operation for this HttpConnection.
 *
 * This is done by simply registering the underlying socket to the the I/O service
 * to watch for available input.
 * \see stop()
 * :
 */
void HttpConnection::start()
{
	if (socket_->state() == Socket::HANDSHAKE)
	{
		//TRACE("start: handshake.");
		socket_->handshake<HttpConnection, &HttpConnection::handshakeComplete>(this);
	}
	else
	{
#if defined(TCP_DEFER_ACCEPT)
		//TRACE("start: read.");
		// it is ensured, that we have data pending, so directly start reading
		processInput();

		// destroy connection in case the above caused connection-close
		// XXX this is usually done within HttpConnection::io(), but we are not.
		if (isClosed())
			delete this;
		else
			active_ = false;
#else
		//TRACE("start: startRead.");
		// client connected, but we do not yet know if we have data pending
		startRead();
#endif
	}
}

void HttpConnection::handshakeComplete(Socket *)
{
	TRACE("handshakeComplete() socketState=%s", socket_->state_str());

	if (socket_->state() == Socket::OPERATIONAL)
		startRead();
	else
	{
		TRACE("handshakeComplete(): handshake failed\n%s", StackTrace().c_str());
		close(); //delete this;
	}
}

inline bool url_decode(BufferRef& url)
{
	std::size_t left = url.offset();
	std::size_t right = left + url.size();
	std::size_t i = left; // read pos
	std::size_t d = left; // write pos
	Buffer& value = url.buffer();

	while (i != right)
	{
		if (value[i] == '%')
		{
			if (i + 3 <= right)
			{
				int ival;
				if (hex2int(value.begin() + i + 1, value.begin() + i + 3, ival))
				{
					value[d++] = static_cast<char>(ival);
					i += 3;
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else if (value[i] == '+')
		{
			value[d++] = ' ';
			++i;
		}
		else if (d != i)
		{
			value[d++] = value[i++];
		}
		else
		{
			++d;
			++i;
		}
	}

	url = value.ref(left, d - left);
	return true;
}

void HttpConnection::messageBegin(BufferRef&& method, BufferRef&& uri, int version_major, int version_minor)
{
	TRACE("messageBegin('%s', '%s', HTTP/%d.%d)", method.str().c_str(), uri.str().c_str(), version_major, version_minor);

	request_ = new HttpRequest(*this);

	request_->method = std::move(method);

	request_->uri = std::move(uri);
	url_decode(request_->uri);

	std::size_t n = request_->uri.find("?");
	if (n != std::string::npos)
	{
		request_->path = request_->uri.ref(0, n);
		request_->query = request_->uri.ref(n + 1);
	}
	else
	{
		request_->path = request_->uri;
	}

	request_->http_version_major = version_major;
	request_->http_version_minor = version_minor;
}

void HttpConnection::messageHeader(BufferRef&& name, BufferRef&& value)
{
	if (iequals(name, "Host"))
	{
		auto i = value.find(':');
		if (i != BufferRef::npos)
			request_->hostname = value.ref(0, i);
		else
			request_->hostname = value;
	}

	request_->headers.push_back(HttpRequestHeader(std::move(name), std::move(value)));
}

bool HttpConnection::messageHeaderEnd()
{
	TRACE("messageHeaderEnd()");
	response_ = new HttpResponse(this);

#if X0_HTTP_STRICT
	BufferRef expectHeader = request_->header("Expect");
	bool content_required = request_->method == "POST" || request_->method == "PUT";

	if (content_required && !request_->content_available())
	{
		response_->status = HttpError::LengthRequired;
		response_->finish();
	}
	else if (!content_required && request_->content_available())
	{
		response_->status = HttpError::BadRequest; // FIXME do we have a better status code?
		response_->finish();
	}
	else if (expectHeader)
	{
		request_->expectingContinue = equals(expectHeader, "100-continue");

		if (!request_->expectingContinue || !request_->supports_protocol(1, 1))
		{
			printf("expectHeader: failed\n");
			response_->status = HttpError::ExpectationFailed;
			response_->finish();
		}
		else
			server_.handleRequest(request_, response_);
	}
	else
		server_.handleRequest(request_, response_);
#else
	server_.handleRequest(request_, response_);
#endif

	return true;
}

bool HttpConnection::messageContent(BufferRef&& chunk)
{
	TRACE("messageContent(#%ld)", chunk.size());

	if (request_)
		request_->on_read(std::move(chunk));

	return true;
}

bool HttpConnection::messageEnd()
{
	TRACE("messageEnd()");

	// increment the number of fully processed requests
	++request_count_;

	// XXX is this really required? (meant to mark the request-content EOS)
	if (request_)
		request_->on_read(BufferRef());

	// allow continueing processing possible further requests
	return true;
}

/** Resumes processing the <b>next</b> HTTP request message within this connection.
 *
 * This method may only be invoked when the current/old request has been fully processed,
 * and is though only be invoked from within the finish handler of \p HttpResponse.
 *
 * \see HttpResponse::finish()
 */
void HttpConnection::resume()
{
	delete response_;
	response_ = 0;

	delete request_;
	request_ = 0;

	// wait for new request message, if nothing in buffer
	if (offset_ != buffer_.size())
		startRead();
}

void HttpConnection::startRead()
{
	int timeout = request_count_ && state() == MESSAGE_BEGIN
		? server_.max_keep_alive_idle()
		: server_.max_read_idle();

	if (timeout > 0)
		socket_->setTimeout<HttpConnection, &HttpConnection::timeout>(this, timeout);

	socket_->setReadyCallback<HttpConnection, &HttpConnection::io>(this);
	socket_->setMode(Socket::READ);
}

/**
 * This method gets invoked when there is data in our HttpConnection ready to read.
 *
 * We assume, that we are in request-parsing state.
 */
void HttpConnection::processInput()
{
	TRACE("(%p).processInput()", this);

	ssize_t rv = socket_->read(buffer_);

	if (rv < 0) // error
	{
		if (errno == EAGAIN || errno == EINTR)
		{
			startRead();
			ev_unloop(loop(), EVUNLOOP_ONE);
		}
		else
		{
			TRACE("processInput(): %s", strerror(errno));
			close();
		}
	}
	else if (rv == 0) // EOF
	{
		TRACE("processInput(): (EOF)");
		close();
	}
	else
	{
		TRACE("processInput(): read %ld bytes", rv);

		//std::size_t offset = buffer_.size();
		//buffer_.resize(offset + rv);

#if !defined(NDEBUG)
//		if (auto cs = std::static_pointer_cast<ConnectionLogger>(custom_data[(HttpPlugin *)this]))
//			cs->log(buffer_.ref(offset, rv));
#endif

		process();

		TRACE("(%p).processInput(): done process()ing; mode=%s, fd=%d, request=%p",
			this, socket_->mode_str(), socket_->handle(), request_);
	}
}

/**
 * Writes as much as it wouldn't block of the response stream into the underlying socket.
 *
 */
void HttpConnection::processOutput()
{
	TRACE("processOutput()");

	for (;;)
	{
		ssize_t rv = sink_.pump(source_);

		TRACE("processOutput(): pump().rv=%ld %s", rv, rv < 0 ? strerror(errno) : "");
		// TODO make use of source_->eof()

		if (rv > 0) // source (partially?) written
		{
			bytesTransferred_ += rv;
		}
		else if (rv == 0) // source fully written
		{
			//TRACE("processOutput(): source fully written");
			source_.reset();

			if (onWriteComplete_)
				onWriteComplete_(0, bytesTransferred_);

			//onWriteComplete_ = CompletionHandlerType();
			break;
		}
		else if (errno == EAGAIN || errno == EINTR) // completing write would block
		{
			socket_->setReadyCallback<HttpConnection, &HttpConnection::io>(this); // XXX should be same
			socket_->setMode(Socket::WRITE);
			break;
		}
		else // an error occurred
		{
			TRACE("processOutput(): write error (%d): %s", errno, strerror(errno));
			source_.reset();

			if (onWriteComplete_)
				onWriteComplete_(errno, bytesTransferred_);

			//onWriteComplete_ = CompletionHandlerType();
			close();
			break;
		}
	}
}

/** closes this HttpConnection, possibly deleting this object (or propagating delayed delete).
 */
void HttpConnection::close()
{
	TRACE("(%p).close() (active=%d)", this, active_);
	TRACE("Stack Trace:%s\n", StackTrace().c_str());

	socket_->close();

	if (!active_)
		delete this;
}

/** processes a (partial) request from buffer's given \p offset of \p count bytes.
 */
void HttpConnection::process()
{
	TRACE("process: offset=%ld, size=%ld (before processing)", offset_, buffer_.size());

	std::error_code ec = HttpMessageProcessor::process(
			buffer_.ref(offset_, buffer_.size() - offset_),
			offset_);

	TRACE("process: offset=%ld, bs=%ld, ec=%s, state=%s (after processing)",
			offset_, buffer_.size(), ec.message().c_str(), state_str());

	if (state() == HttpMessageProcessor::MESSAGE_BEGIN)
	{
#if 0
		offset_ = 0;
		buffer_.clear();
#endif
	}

	if (isClosed())
		return;

	if (ec == HttpMessageError::partial)
		startRead();
	else if (ec && ec != HttpMessageError::aborted)
	{
		// -> send stock response: BAD_REQUEST
		if (!response_)
			response_ = new HttpResponse(this, HttpError::BadRequest);
		else
			response_->status = HttpError::BadRequest;

		response_->finish();
	}
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
	return listener_.address();
}

unsigned int HttpConnection::localPort() const
{
	return socket_->localPort();
}

} // namespace x0
