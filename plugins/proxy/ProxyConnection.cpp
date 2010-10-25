/* <x0/plugins/proxy/ProxyConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "ProxyConnection.h"
#include "ProxyContext.h"

#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
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
#	define TRACE(msg...) /*!*/
#else
#	define TRACE(msg...) DEBUG("ProxyConnection: " msg)
#endif

ProxyConnection::ProxyConnection(ProxyContext *px) :
	WebClientBase(px->loop),
	px_(px),
	done_(),
	request_(NULL),
	response_(NULL)
{
}

/** callback, invoked once the connection to the origin server is established.
 *
 * Though, passing the request message to the origin server.
 */
void ProxyConnection::connect()
{
	TRACE("connection(%p).connect()", this);
	if (!response_)
		return;

	pass_request();
}

/** callback, invoked when the origin server has passed us the response status line.
 *
 * We will use the status code only.
 * However, we could pass the text field, too - once x0 core supports it.
 */
void ProxyConnection::response(int major, int minor, int code, x0::BufferRef&& text)
{
	TRACE("ProxyConnection(%p).status(HTTP/%d.%d, %d, '%s')", this, major, minor, code, text.str().c_str());
	response_->status = static_cast<x0::HttpError>(code);
}

inline bool validate_response_header(const x0::BufferRef& name)
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
void ProxyConnection::header(x0::BufferRef&& name, x0::BufferRef&& value)
{
	TRACE("ProxyConnection(%p).header('%s', '%s')", this, name.str().c_str(), value.str().c_str());

	if (validate_response_header(name))
		response_->responseHeaders.push_back(name.str(), value.str());
}

/** callback, invoked when a new content chunk from origin has arrived.
 *
 * We temporarily pause the client to not receive any more data
 * until having fully transmitted the currently passed one to the actual client.
 *
 * The client must be resumed once the current chunk has been fully passed
 * to the client.
 */
bool ProxyConnection::content(x0::BufferRef&& chunk)
{
	TRACE("ProxyConnection(%p).content(size=%ld)", this, chunk.size());

	pause();

	response_->write(std::make_shared<x0::BufferSource>(chunk),
			std::bind(&ProxyConnection::content_written, this, std::placeholders::_1, std::placeholders::_2));

	return true;
}

/** callback, invoked once the origin's response message has been fully received.
 *
 * We will inform x0 core, that we've finished processing this request and destruct ourselfs.
 */
bool ProxyConnection::complete()
{
	TRACE("ProxyConnection(%p).complete()", this);

	if (static_cast<int>(response_->status) == 0)
		response_->status = x0::HttpError::ServiceUnavailable;

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
void ProxyConnection::content_written(int ec, std::size_t nb)
{
	TRACE("connection(%p).content_written(ec=%d, nb=%ld): %s", this, ec, nb, ec ? strerror(errno) : "");

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
			TRACE("ProxyConnection(%p): connect error: %s", this, last_error().message().c_str());
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
 * \param in Corresponding request.
 * \param out Corresponding response.
 */
void ProxyConnection::start(const std::function<void()>& done, x0::HttpRequest *in, x0::HttpResponse *out)
{
	TRACE("connection(%p).start(): path=%s (state()=%d)", this, in->path.str().c_str(), state());

	if (state() == DISCONNECTED)
	{
		out->status = x0::HttpError::ServiceUnavailable;
		done();
		delete this;
		return;
	}

	done_ = done;
	request_ = in;
	response_ = out;

	if (state() == CONNECTED)
		pass_request();
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
void ProxyConnection::pass_request()
{
	TRACE("connection(%p).pass_request('%s', '%s', '%s')", this, 
		request_->method.str().c_str(), request_->path.str().c_str(), request_->query.str().c_str());

	// request line
	if (request_->query)
		write_request(request_->method, request_->path, request_->query);
	else
		write_request(request_->method, request_->path);

	// request-headers
	for (auto i = request_->requestHeaders.begin(), e = request_->requestHeaders.end(); i != e; ++i)
		if (validate_request_header(i->name))
			write_header(i->name, i->value);

	if (!hostname_.empty())
	{
		write_header("Host", hostname_);
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

		write_header("Host", result);
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
		request_->read(std::bind(&ProxyConnection::pass_request_content, this, _1));
	}
}

/** callback, invoked when client content chunk is available, and is to pass to the origin server.
 */
void ProxyConnection::pass_request_content(x0::BufferRef&& chunk)
{
	TRACE("ProxyConnection.pass_request_content(): '%s'", chunk.str().c_str());
	//client.write(chunk, std::bind(&ProxyConnection::request_content_passed, this);
}
