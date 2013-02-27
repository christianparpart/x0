/* <x0/plugins/proxy.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/http/HttpPlugin.h>
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

/* {{{ -- configuration proposal:
 *
 * handler setup {
 * }
 *
 * handler main {
 *     proxy.reverse 'http://127.0.0.1:3000';
 * }
 *
 * --------------------------------------------------------------------------
 * possible tweaks:
 *  - bufsize (0 = unbuffered)
 *  - timeout.connect
 *  - timeout.write
 *  - timeout.read
 *  - ignore_clientabort
 * };
 *
 *
 */ // }}}

#if 0
#	define TRACE(msg...) DEBUG("proxy: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif

// {{{ ProxyConnection API
class ProxyConnection :
	public x0::HttpMessageProcessor
{
private:
	int refCount_;

	x0::HttpRequest *request_;		//!< client's request
	x0::Socket* backend_;			//!< connection to backend app
	bool cloak_;					//!< to cloak or not to cloak the "Server" response header

	int connectTimeout_;
	int readTimeout_;
	int writeTimeout_;

	x0::Buffer writeBuffer_;
	size_t writeOffset_;
	size_t writeProgress_;

	x0::Buffer readBuffer_;
	bool processingDone_;

private:
	void ref();
	void unref();
	inline void close();
	inline void readSome();
	inline void writeSome();

	void onConnected(x0::Socket* s, int revents);
	void io(x0::Socket* s, int revents);
	void onRequestChunk(const x0::BufferRef& chunk);

	static void onAbort(void *p);
	void onWriteComplete();

	// response (HttpMessageProcessor)
	virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const x0::BufferRef& text);
	virtual bool onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool onMessageContent(const x0::BufferRef& chunk);
	virtual bool onMessageEnd();

	virtual void log(x0::LogMessage&& msg)
	{
		if (request_) {
			msg.addTag("proxy");
			request_->log(std::move(msg));
		}
	}
public:
	inline ProxyConnection();
	~ProxyConnection();

	void start(x0::HttpRequest* in, x0::Socket* backend, bool cloak);
};
// }}}

// {{{ ProxyConnection impl
ProxyConnection::ProxyConnection() :
	x0::HttpMessageProcessor(x0::HttpMessageProcessor::RESPONSE),
	refCount_(1),
	request_(nullptr),
	backend_(nullptr),
	cloak_(false),

	connectTimeout_(0),
	readTimeout_(0),
	writeTimeout_(0),
	writeBuffer_(),
	writeOffset_(0),
	writeProgress_(0),
	readBuffer_(),
	processingDone_(false)
{
	TRACE("ProxyConnection()");
}

ProxyConnection::~ProxyConnection()
{
	TRACE("~ProxyConnection()");

	if (backend_) {
		backend_->close();

		delete backend_;
	}

	if (request_) {
		if (request_->status == x0::HttpStatus::Undefined)
			request_->status = x0::HttpStatus::ServiceUnavailable;

		request_->finish();
	}
}

void ProxyConnection::ref()
{
	++refCount_;
}

void ProxyConnection::unref()
{
	assert(refCount_ > 0);

	--refCount_;

	if (refCount_ == 0) {
		delete this;
	}
}

void ProxyConnection::close()
{
	if (backend_)
		// stop watching on any backend I/O events, if active
		backend_->close();

	unref(); // the one from the constructor
}

void ProxyConnection::onAbort(void *p)
{
	ProxyConnection *self = reinterpret_cast<ProxyConnection *>(p);
	self->close();
}

void ProxyConnection::start(x0::HttpRequest* in, x0::Socket* backend, bool cloak)
{
	TRACE("ProxyConnection.start(in, backend, cloak=%d)", cloak);

	request_ = in;
	request_->setAbortHandler(&ProxyConnection::onAbort, this);
	backend_ = backend;
	cloak_ = cloak;

	// request line
	writeBuffer_.push_back(request_->method);
	writeBuffer_.push_back(' ');
	writeBuffer_.push_back(request_->unparsedUri);
	writeBuffer_.push_back(" HTTP/1.1\r\n");

	x0::BufferRef forwardedFor;

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

	if (backend_->state() == x0::Socket::Connecting) {
		TRACE("start: connect in progress");
		backend_->setReadyCallback<ProxyConnection, &ProxyConnection::onConnected>(this);
	} else { // connected
		TRACE("start: flushing");
		backend_->setReadyCallback<ProxyConnection, &ProxyConnection::io>(this);
		backend_->setMode(x0::Socket::ReadWrite);
	}
}

void ProxyConnection::onConnected(x0::Socket* s, int revents)
{
	TRACE("onConnected: content? %d", request_->contentAvailable());
	//TRACE("onConnected.pending:\n%s\n", writeBuffer_.c_str());

	if (backend_->state() == x0::Socket::Operational) {
		TRACE("onConnected: flushing");
		backend_->setReadyCallback<ProxyConnection, &ProxyConnection::io>(this);
		backend_->setMode(x0::Socket::ReadWrite); // flush already serialized request
	} else {
		close();
	}
}

/** transferres a request body chunk to the origin server.  */
void ProxyConnection::onRequestChunk(const x0::BufferRef& chunk)
{
	TRACE("onRequestChunk(nb:%ld)", chunk.size());
	writeBuffer_.push_back(chunk);

	if (backend_->state() == x0::Socket::Operational) {
		backend_->setMode(x0::Socket::ReadWrite);
	}
}

inline bool validateResponseHeader(const x0::BufferRef& name)
{
	// XXX do not allow origin's connection-level response headers to be passed to the client.
	if (iequals(name, "Connection"))
		return false;

	if (iequals(name, "Transfer-Encoding"))
		return false;

	return true;
}

/** callback, invoked when the origin server has passed us the response status line.
 *
 * We will use the status code only.
 * However, we could pass the text field, too - once x0 core supports it.
 */
bool ProxyConnection::onMessageBegin(int major, int minor, int code, const x0::BufferRef& text)
{
	TRACE("ProxyConnection(%p).status(HTTP/%d.%d, %d, '%s')", (void*)this, major, minor, code, text.str().c_str());

	request_->status = static_cast<x0::HttpStatus>(code);
	TRACE("status: %d", (int)request_->status);
	return true;
}

/** callback, invoked on every successfully parsed response header.
 *
 * We will pass this header directly to the client's response,
 * if that is NOT a connection-level header.
 */
bool ProxyConnection::onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value)
{
	TRACE("ProxyConnection(%p).onHeader('%s', '%s')", (void*)this, name.str().c_str(), value.str().c_str());

	if (!validateResponseHeader(name)) {
		TRACE("skipping connection-level header");
		goto done;
	}

	if (cloak_ && iequals(name, "Server")) {
		TRACE("skipping \"Server\"-header");
		goto done;
	}

	request_->responseHeaders.push_back(name.str(), value.str());

done:
	return true;
}

/** callback, invoked on a new response content chunk. */
bool ProxyConnection::onMessageContent(const x0::BufferRef& chunk)
{
	TRACE("messageContent(nb:%lu) state:%s", chunk.size(), backend_->state_str());

	// stop watching for more input
	backend_->setMode(x0::Socket::None);

	// transfer response-body chunk to client
	request_->write<x0::BufferRefSource>(chunk);

	// start listening on backend I/O when chunk has been fully transmitted
	ref();
	request_->writeCallback<ProxyConnection, &ProxyConnection::onWriteComplete>(this);

	return true;
}

void ProxyConnection::onWriteComplete()
{
	TRACE("chunk write complete: %s", state_str());
	backend_->setMode(x0::Socket::Read);
	unref();
}

bool ProxyConnection::onMessageEnd()
{
	TRACE("messageEnd() backend-state:%s", backend_->state_str());
	processingDone_ = true;
	return false;
}

void ProxyConnection::io(x0::Socket* s, int revents)
{
	TRACE("io(0x%04x)", revents);

	if (revents & x0::Socket::Read)
		readSome();

	if (revents & x0::Socket::Write)
		writeSome();
}

void ProxyConnection::writeSome()
{
	TRACE("writeSome() - %s (%d)", state_str(), request_->contentAvailable());

	ssize_t rv = backend_->write(writeBuffer_.data() + writeOffset_, writeBuffer_.size() - writeOffset_);

	if (rv > 0) {
		TRACE("write request: %ld (of %ld) bytes", rv, writeBuffer_.size() - writeOffset_);

		writeOffset_ += rv;
		writeProgress_ += rv;

		if (writeOffset_ == writeBuffer_.size()) {
			TRACE("writeOffset == writeBuffser.size (%ld) p:%ld, ca: %d, clr:%ld", writeOffset_,
				writeProgress_, request_->contentAvailable(), request_->connection.contentLength());

			writeOffset_ = 0;
			writeBuffer_.clear();
			backend_->setMode(x0::Socket::Read);
		}
	} else {
		TRACE("write request failed(%ld): %s", rv, strerror(errno));
		close();
	}
}

void ProxyConnection::readSome()
{
	TRACE("readSome() - %s", state_str());

	std::size_t lower_bound = readBuffer_.size();

	if (lower_bound == readBuffer_.capacity())
		readBuffer_.setCapacity(lower_bound + 4096);

	ssize_t rv = backend_->read(readBuffer_);

	if (rv > 0) {
		TRACE("read response: %ld bytes", rv);
		std::size_t np = process(readBuffer_.ref(lower_bound, rv));
		(void) np;
		TRACE("readSome(): process(): %ld / %ld", np, rv);

		if (processingDone_ || state() == SYNTAX_ERROR) {
			close();
		} else {
			TRACE("resume with io:%d, state:%s", backend_->mode(), backend_->state_str());
			backend_->setMode(x0::Socket::Read);
		}
	} else if (rv == 0) {
		TRACE("http server connection closed");
		close();
	} else {
		TRACE("read response failed(%ld): %s", rv, strerror(errno));

		switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		case EWOULDBLOCK:
#endif
		case EAGAIN:
		case EINTR:
			backend_->setMode(x0::Socket::Read);
			break;
		default:
			close();
			break;
		}
	}
}

// }}}

// {{{ plugin class
/**
 * \ingroup plugins
 * \brief proxy content generator plugin
 */
class proxy_plugin :
	public x0::HttpPlugin
{
private:
	bool cloak_;

public:
	proxy_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		cloak_(true)
	{
		registerHandler<proxy_plugin, &proxy_plugin::proxy_reverse>("proxy.reverse");
		registerSetupProperty<proxy_plugin, &proxy_plugin::proxy_cloak>("proxy.cloak", x0::FlowValue::BOOLEAN);
	}

	~proxy_plugin()
	{
	}

private:
	void proxy_cloak(const x0::FlowParams& args, x0::FlowValue& result)
	{
		if (args.size() && (args[0].isBool() || args[0].isNumber())) {
			cloak_ = args[0].toBool();
			TRACE("proxy cloak: %s", cloak_ ? "true" : "false");
		}

		result.set(cloak_);
	}

	bool proxy_reverse(x0::HttpRequest *in, const x0::FlowParams& args)
	{
		// TODO: reuse already spawned proxy connections instead of recreating each time.
		x0::SocketSpec spec;
		spec << args;
		if (!spec.isValid() || spec.backlog() >= 0) {
			in->log(x0::Severity::error, "Invalid socket spec passed.");
			return false;
		}

		if (x0::Socket* backend = x0::Socket::open(in->connection.worker().loop(), spec, O_NONBLOCK | O_CLOEXEC)) {
			TRACE("in.content? %d", in->contentAvailable());
			if (ProxyConnection* pc = new ProxyConnection()) {
				pc->start(in, backend, cloak_);
				return true;
			}
		}

		in->status = x0::HttpStatus::ServiceUnavailable;
		in->finish();

		return true;
	}
};
// }}}

X0_EXPORT_PLUGIN(proxy)
