/* <x0/HttpResponse.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpResponse.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpServer.h>
#include <x0/io/FileSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/ChunkedEncoder.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <strings.h>						// strcasecmp
#include <string>
#include <algorithm>

#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("HttpResponse: " msg)
#endif

namespace x0 {

using boost::algorithm::iequals;

char HttpResponse::status_codes[512][4];

HttpResponse::~HttpResponse()
{
	//TRACE("~HttpResponse(%p, conn=%p)", this, connection_);
}

template<typename T>
inline std::string make_str(T value)
{
	return boost::lexical_cast<std::string>(value);
}

/** populates a default-response content, possibly modifying a few response headers, too.
 *
 * \return the newly created response content or a null ptr if the statuc code forbids content.
 *
 * \note Modified headers are "Content-Type" and "Content-Length".
 */
SourcePtr HttpResponse::make_default_content()
{
	if (content_forbidden())
		return SourcePtr();

	// TODO custom error documents
#if 0
	std::string filename(connection_->server().config()["ErrorDocuments"][make_str(status)].as<std::string>());
	FileInfoPtr fi(connection_->server().fileinfo(filename));
	int fd = ::open(fi->filename().c_str(), O_RDONLY);
	if (fi->exists() && fd >= 0)
	{
		headers.overwrite("Content-Type", fi->mimetype());
		headers.overwrite("Content-Length", boost::lexical_cast<std::string>(fi->size()));

		return std::make_shared<FileSource>(fd, 0, fi->size(), true);
	}
	else
#endif
	{
		std::string codeStr = http_category().message(static_cast<int>(status));
		char buf[1024];

		int nwritten = snprintf(buf, sizeof(buf),
			"<html>"
			"<head><title>%s</title></head>"
			"<body><h1>%d %s</h1></body>"
			"</html>\r\n",
			codeStr.c_str(), status, codeStr.c_str()
		);

		headers.overwrite("Content-Type", "text/html");
		headers.overwrite("Content-Length", boost::lexical_cast<std::string>(nwritten));

		return std::make_shared<BufferSource>(Buffer::from_copy(buf, nwritten));
	}
}

/** serializes the HTTP response status line plus headers into a byte-stream.
 *
 * This method is invoked right before the response content is written or the
 * response is flushed at all.
 *
 * It first sets the status code (if not done yet), invoked post_process callback,
 * performs connection-level response header modifications and then
 * builds the response chunk for status line and headers.
 *
 * Post-modification done <b>after</b> the post_process hook has been invoked:
 * <ol>
 *   <li>set status code to 200 (Ok) if not set yet.</li>
 *   <li>set Content-Type header to a default if not set yet.</li>
 *   <li>set Connection header to keep-alive or close (computed value)</li>
 *   <li>append Transfer-Encoding chunked if no Content-Length header is set.</li>
 *   <li>optionally enable TCP_CORK if this is no keep-alive connection and the administrator allows it.</li>
 * </ol>
 *
 * \note this does not serialize the message body.
 */
SourcePtr HttpResponse::serialize()
{
	Buffer buffers;
	bool keepalive = false;

	if (request_->expectingContinue)
		status = HttpError::ExpectationFailed;
	else if (status == static_cast<HttpError>(0))
		status = HttpError::Ok;

	if (!headers.contains("Content-Type"))
	{
		headers.push_back("Content-Type", "text/plain"); //!< \todo pass "default" content-type instead!
	}

	// post-response hook
	connection_->worker().server().onPostProcess(const_cast<HttpRequest *>(request_), this);

	// setup (connection-level) response transfer
	if (!headers.contains("Content-Length") && !content_forbidden())
	{
		if (request_->supports_protocol(1, 1)
			&& equals(request_->header("Connection"), "keep-alive")
			&& !headers.contains("Transfer-Encoding")
			&& !content_forbidden())
		{
			headers.overwrite("Connection", "keep-alive");
			headers.push_back("Transfer-Encoding", "chunked");
			filters.push_back(std::make_shared<ChunkedEncoder>());
			keepalive = true;
		}
	}
	else if (!headers.contains("Connection"))
	{
		if (iequals(request_->header("Connection"), "keep-alive"))
		{
			headers.push_back("Connection", "keep-alive");
			keepalive = true;
		}
	}
	else if (iequals(headers("Connection"), "keep-alive"))
	{
		keepalive = true;
	}

	if (!connection_->worker().server().max_keep_alive_idle())
		keepalive = false;

	keepalive = false; // XXX workaround

	if (!keepalive)
		headers.overwrite("Connection", "close");

	if (!keepalive && connection_->worker().server().tcp_cork())
		connection_->socket()->setTcpCork(true);

	if (request_->supports_protocol(1, 1))
		buffers.push_back("HTTP/1.1 ");
	else if (request_->supports_protocol(1, 0))
		buffers.push_back("HTTP/1.0 ");
	else
		buffers.push_back("HTTP/0.9 ");

	buffers.push_back(status_codes[static_cast<int>(status)]);
	buffers.push_back(' ');
	buffers.push_back(status_str(status));
	buffers.push_back("\r\n");

	for (auto i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		const HttpResponseHeader& h = *i;
		buffers.push_back(h.name.data(), h.name.size());
		buffers.push_back(": ");
		buffers.push_back(h.value.data(), h.value.size());
		buffers.push_back("\r\n");
	};

	buffers.push_back("\r\n");

	return std::make_shared<BufferSource>(std::move(buffers));
}

/** Creates an empty response object.
 *
 * \param connection the connection this response is going to be transmitted through
 * \param request the corresponding request object. <b>We take over ownership of it!</b>
 * \param status initial response status code.
 *
 * \note this response object takes over ownership of the request object.
 */
HttpResponse::HttpResponse(HttpConnection *connection, HttpError _status) :
	connection_(connection),
	request_(connection_->request_),
	headers_sent_(false),
	status(_status),
	headers()
{
	//TRACE("HttpResponse(%p, conn=%p)", this, connection_);

	// TODO: use worker::now() instead!
	headers.push_back("Date", connection_->worker().now().http_str().str());

	if (connection_->worker().server().advertise() && !connection_->worker().server().tag().empty())
		headers.push_back("Server", connection_->worker().server().tag());
}

std::string HttpResponse::status_str(HttpError value)
{
	return http_category().message(static_cast<int>(value));
}

/** completion handler, being invoked when this response has been fully flushed and is considered done.
 * \see HttpResponse::finish()
 */
void HttpResponse::onFinished(int ec)
{
	//TRACE("HttpResponse(%p).onFinished(%d)", this, ec);

	{
		HttpServer& srv = request_->connection.worker().server();

		// log request/response
		srv.onRequestDone(const_cast<HttpRequest *>(request_), this);
	}

	// close, if not a keep-alive connection
	if (iequals(headers["Connection"], "keep-alive"))
		connection_->resume();
	else
		connection_->close();
}

/** finishes this response by flushing the content into the stream.
 *
 * \note this also queues the underlying connection for processing the next request (on keep-alive).
 */
void HttpResponse::finish()
{
	if (!headers_sent_) // nothing sent to client yet -> sent default status page
	{
		if (static_cast<int>(status) == 0)
			status = HttpError::NotFound;

		if (!content_forbidden() && status != HttpError::Ok)
			write(make_default_content(), std::bind(&HttpResponse::onFinished, this, std::placeholders::_1));
		else
			connection_->writeAsync(serialize(), std::bind(&HttpResponse::onFinished, this, std::placeholders::_1));
	}
	else if (!filters.empty())
	{
		// mark the end of stream (EOF) by passing an empty chunk to the filters.
		connection_->writeAsync(
			std::make_shared<FilterSource>(std::make_shared<BufferSource>(""), filters, true),
			std::bind(&HttpResponse::onFinished, this, std::placeholders::_1)
		);
	}
	else
	{
		onFinished(0);
	}
}

/** to be called <b>once</b> in order to initialize this class for instanciation.
 *
 * \note to be invoked by HttpServer constructor.
 * \see server
 */
void HttpResponse::initialize()
{
	// pre-compute string representations of status codes for use in response serialization
	for (std::size_t i = 0; i < sizeof(status_codes) / sizeof(*status_codes); ++i)
		snprintf(status_codes[i], sizeof(*status_codes), "%03ld", i);
}

} // namespace x0
