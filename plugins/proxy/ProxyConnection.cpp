/* <x0/plugins/proxy/ProxyConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "ProxyConnection.h"

#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <x0/strutils.h>
#include <x0/Url.h>
#include <x0/Types.h>

#include <cstring>
#include <cerrno>
#include <cstddef>
#include <deque>

#include <ev++.h>

#if 1
#	define TRACE(msg...) DEBUG("ProxyConnection: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif

ProxyConnection::ProxyConnection(struct ev_loop *loop) :
	WebClientBase(loop),
	done_(),
	request_(NULL)
{
}

/** callback, invoked once the connection to the origin server is established.
 *
 * Though, passing the request message to the origin server.
 */
void ProxyConnection::onConnect()
{
	TRACE("connection(%p).connect()", this);
	if (!request_)
		return;

	passRequest();
}

/** callback, invoked when the origin server has passed us the response status line.
 *
 * We will use the status code only.
 * However, we could pass the text field, too - once x0 core supports it.
 */
void ProxyConnection::onResponse(int major, int minor, int code, x0::BufferRef&& text)
{
	TRACE("ProxyConnection(%p).status(HTTP/%d.%d, %d, '%s')", this, major, minor, code, text.str().c_str());
	request_->status = static_cast<x0::HttpError>(code);
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

/** callback, invoked on every successfully parsed response header.
 *
 * We will pass this header directly to the client's response if
 * that is NOT a connection-level header.
 */
void ProxyConnection::onHeader(x0::BufferRef&& name, x0::BufferRef&& value)
{
	TRACE("ProxyConnection(%p).onHeader('%s', '%s')", this, name.str().c_str(), value.str().c_str());

	if (validateResponseHeader(name))
		request_->responseHeaders.push_back(name.str(), value.str());
}

/** callback, invoked when a new content chunk from origin has arrived.
 *
 * We temporarily pause the client to not receive any more data
 * until having fully transmitted the currently passed one to the actual client.
 *
 * The client must be resumed once the current chunk has been fully passed
 * to the client.
 */
bool ProxyConnection::onContentChunk(x0::BufferRef&& chunk)
{
	TRACE("ProxyConnection(%p).onContentChunk(size=%ld)", this, chunk.size());

	pause();

	request_->write(std::make_shared<x0::BufferSource>(chunk),
			std::bind(&ProxyConnection::onContentWritten, this, std::placeholders::_1, std::placeholders::_2));

	return true;
}

/** callback, invoked once the origin's response message has been fully received.
 *
 * We will inform x0 core, that we've finished processing this request and destruct ourselfs.
 */
bool ProxyConnection::onComplete()
{
	TRACE("ProxyConnection(%p).onComplete()", this);

	if (static_cast<int>(request_->status) == 0)
		request_->status = x0::HttpError::ServiceUnavailable;

	done_();
	delete this;

	// do not continue processing
	return false;
}

/** completion handler, invoked when response content chunk has been sent to the client.
 *
 * If the previousely transferred chunk has been successfully written, we will
 * resume receiving response content from the origin server, or kill us otherwise.
 */
void ProxyConnection::onContentWritten(int ec, std::size_t nb)
{
	TRACE("connection(%p).onContentWritten(ec=%d, nb=%ld): %s", this, ec, nb, ec ? strerror(errno) : "");

	if (!ec)
	{
		resume();
	}
	else
	{
		ec = errno;
		request_->log(x0::Severity::info, "proxy: client %s aborted with %s.",
				request_->connection.remoteIP().c_str(), strerror(ec));

		delete this;
	}
}

ProxyConnection::~ProxyConnection()
{
	TRACE("~ProxyConnection(%p) destroy", this);
}

/** Asynchronously connects to origin server.
 *
 * \param origin the origin's HTTP URL.
 *
 * \note this can also result into a non-async connect if it would not block.
 */
void ProxyConnection::connect(const std::string& origin)
{
	std::string protocol;
	if (!x0::parseUrl(origin, protocol, hostname_, port_))
	{
		TRACE("ProxyConnection(%p).connect() failed: %s.", this, "Origin URL parse error");
		return;
	}

	TRACE("ProxyConnection(%p): connecting to %s port %d", this, hostname_.c_str(), port_);

	open(hostname_, port_);

	switch (state())
	{
		case DISCONNECTED:
			TRACE("ProxyConnection(%p): connect error: %s", this, lastError().message().c_str());
			break;
		default:
			break;
	}
}

/** Disconnects from origin server, possibly finishing client response aswell.
 */
void ProxyConnection::disconnect()
{
	close();
}

/** Starts processing the client request.
 *
 * \param done Callback to invoke when request has been fully processed (or an error occurred).
 * \param r Corresponding HTTP request object.
 */
void ProxyConnection::start(const std::function<void()>& done, x0::HttpRequest *r)
{
	TRACE("connection(%p).start(): path=%s (state()=%d)", this, r->path.str().c_str(), state());

	if (state() == DISCONNECTED)
	{
		r->status = x0::HttpError::ServiceUnavailable;
		done();
		delete this;
		return;
	}

	done_ = done;
	request_ = r;

	if (state() == CONNECTED)
		passRequest();
}

/** test whether or not this request header may be passed to the origin server.
 */
inline bool validate_request_header(const x0::BufferRef& name)
{
	if (x0::iequals(name, "Host"))
		return false;

	if (x0::iequals(name, "Accept-Encoding"))
		return false;

	if (x0::iequals(name, "Connection"))
		return false;

	if (x0::iequals(name, "Keep-Alive"))
		return false;

	return true;
}

/** startss passing the client request message to the origin server.
 */
void ProxyConnection::passRequest()
{
	TRACE("connection(%p).passRequest('%s', '%s', '%s')", this, 
		request_->method.str().c_str(), request_->path.str().c_str(), request_->query.str().c_str());

	// request line
	if (request_->query)
		writeRequest(request_->method, request_->path, request_->query);
	else
		writeRequest(request_->method, request_->path);

	// request-headers
	for (auto i = request_->requestHeaders.begin(), e = request_->requestHeaders.end(); i != e; ++i)
		if (validate_request_header(i->name))
			writeHeader(i->name, i->value);

	if (!hostname_.empty())
	{
		writeHeader("Host", hostname_);
	}
	else
	{
		x0::Buffer result;

		auto hostid = request_->hostid();
		std::size_t n = hostid.find(':');
		if (n != std::string::npos)
			result.push_back(hostid.substr(0, n));
		else
			result.push_back(hostid);

		result.push_back(':');
		result.push_back(port_);

		writeHeader("Host", result);
	}

	//! \todo body?
#if 0
	if (request_->body.empty())
	{
		;//! \todo properly handle POST data (request_->body)
	}
#endif

	commit(true);

	if (request_->contentAvailable())
	{
		using namespace std::placeholders;
		request_->read(std::bind(&ProxyConnection::passRequestContent, this, _1));
	}
}

/** callback, invoked when client content chunk is available, and is to pass to the origin server.
 */
void ProxyConnection::passRequestContent(x0::BufferRef&& chunk)
{
	TRACE("ProxyConnection.passRequestContent(): '%s'", chunk.str().c_str());
	//client.write(chunk, std::bind(&ProxyConnection::request_content_passed, this);
}
