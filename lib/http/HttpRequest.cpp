/* <x0/HttpRequest.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpRequest.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpListener.h>
#include <x0/io/BufferSource.h>
#include <x0/io/FilterSource.h>
#include <x0/io/ChunkedEncoder.h>
#include <boost/algorithm/string.hpp>
#include <x0/strutils.h>
#include <strings.h>			// strcasecmp()

#define TRACE(msg...) DEBUG(msg)

namespace x0 {

using boost::algorithm::iequals;

char HttpRequest::statusCodes_[512][4];

HttpRequest::HttpRequest(HttpConnection& conn) :
	headersSent_(false),
	connection(conn),
	method(),
	uri(),
	path(),
	fileinfo(),
	pathinfo(),
	query(),
	httpVersionMajor(0),
	httpVersionMinor(0),
	requestHeaders(),
	username(),
	documentRoot(),
	expectingContinue(false),
	customData(),

	status(),
	responseHeaders(),
	outputFilters(),

	hostid_(),
	readCallback_()
{
	responseHeaders.push_back("Date", connection.worker().now().http_str().str());

	if (connection.worker().server().advertise() && !connection.worker().server().tag().empty())
		responseHeaders.push_back("Server", connection.worker().server().tag());
}

void HttpRequest::updatePathInfo()
{
	if (!fileinfo)
		return;

	// split "/the/tail" from "/path/to/script.php/the/tail"

	std::string fullname(fileinfo->filename());
	struct stat st;
	size_t pos = std::string::npos;

	for (;;)
	{
		int rv = stat(fullname.c_str(), &st);
		if (rv == 0)
		{
			pathinfo = pos != std::string::npos ? fileinfo->filename().substr(pos) : "";
			fileinfo = connection.worker().fileinfo(fullname);
			return;
		}
		if (errno == ENOTDIR)
		{
			pos = fullname.rfind('/', pos - 1);
			fullname = fullname.substr(0, pos);
		}
		else
		{
			return;
		}
	}
}

BufferRef HttpRequest::requestHeader(const std::string& name) const
{
	for (std::vector<HttpRequestHeader>::const_iterator i = requestHeaders.begin(), e = requestHeaders.end(); i != e; ++i)
		if (iequals(i->name, name))
			return i->value;

	return BufferRef();
}

std::string HttpRequest::hostid() const
{
	if (hostid_.empty())
		hostid_ = x0::make_hostid(requestHeader("Host"), connection.listener().port());

	return hostid_;
}

void HttpRequest::setHostid(const std::string& value)
{
	hostid_ = value;
}

bool HttpRequest::contentAvailable() const
{
	return connection.state() != HttpMessageProcessor::MESSAGE_BEGIN;
}

/** setup request-body consumer callback.
 *
 * \param callback the callback to invoke on request-body chunks.
 *
 * \retval true callback set
 * \retval false callback not set (because there is no content available)
 */
bool HttpRequest::read(const std::function<void(BufferRef&&)>& callback)
{
	if (!contentAvailable())
		return false;

	if (expectingContinue)
	{
		TRACE("send 100-continue");

		connection.write<BufferSource>("HTTP/1.1 100 Continue\r\n\r\n");
		expectingContinue = false;
	}

	readCallback_ = callback;

	return true;
}

void HttpRequest::onRequestContent(BufferRef&& chunk)
{
	if (readCallback_)
	{
		readCallback_(std::move(chunk));
		readCallback_ = std::function<void(BufferRef&&)>();
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
SourcePtr HttpRequest::serialize()
{
	Buffer buffers;
	bool keepalive = false;

	if (expectingContinue)
		status = HttpError::ExpectationFailed;
	else if (status == static_cast<HttpError>(0))
		status = HttpError::Ok;

	if (!responseHeaders.contains("Content-Type"))
	{
		responseHeaders.push_back("Content-Type", "text/plain"); //!< \todo pass "default" content-type instead!
	}

	// post-response hook
	connection.worker().server().onPostProcess(this);

	// setup (connection-level) response transfer
	if (!responseHeaders.contains("Content-Length") && !responseContentForbidden())
	{
		if (supportsProtocol(1, 1)
			&& equals(requestHeader("Connection"), "keep-alive")
			&& !responseHeaders.contains("Transfer-Encoding")
			&& !responseContentForbidden())
		{
			responseHeaders.overwrite("Connection", "keep-alive");
			responseHeaders.push_back("Transfer-Encoding", "chunked");
			outputFilters.push_back(std::make_shared<ChunkedEncoder>());
			keepalive = true;
		}
	}
	else if (!responseHeaders.contains("Connection"))
	{
		if (iequals(requestHeader("Connection"), "keep-alive"))
		{
			responseHeaders.push_back("Connection", "keep-alive");
			keepalive = true;
		}
	}
	else if (iequals(responseHeaders["Connection"], "keep-alive"))
	{
		keepalive = true;
	}

	if (!connection.worker().server().max_keep_alive_idle())
		keepalive = false;

	keepalive = false; // XXX workaround

	if (!keepalive)
		responseHeaders.overwrite("Connection", "close");

	if (!connection.worker().server().tcp_cork())
		connection.socket()->setTcpCork(true);

	if (supportsProtocol(1, 1))
		buffers.push_back("HTTP/1.1 ");
	else if (supportsProtocol(1, 0))
		buffers.push_back("HTTP/1.0 ");
	else
		buffers.push_back("HTTP/0.9 ");

	buffers.push_back(statusCodes_[static_cast<int>(status)]);
	buffers.push_back(' ');
	buffers.push_back(statusStr(status));
	buffers.push_back("\r\n");

	for (auto i = responseHeaders.begin(), e = responseHeaders.end(); i != e; ++i)
	{
		const HeaderList::Header& h = *i;
		buffers.push_back(h.name.data(), h.name.size());
		buffers.push_back(": ");
		buffers.push_back(h.value.data(), h.value.size());
		buffers.push_back("\r\n");
	};

	buffers.push_back("\r\n");

	return std::make_shared<BufferSource>(std::move(buffers));
}

/** populates a default-response content, possibly modifying a few response headers, too.
 *
 * \return the newly created response content or a null ptr if the statuc code forbids content.
 *
 * \note Modified headers are "Content-Type" and "Content-Length".
 */
SourcePtr HttpRequest::makeDefaultResponseContent()
{
	if (responseContentForbidden())
		return SourcePtr();

	// TODO custom error documents
#if 0
	std::string filename(connection.server().config()["ErrorDocuments"][make_str(status)].as<std::string>());
	FileInfoPtr fi(connection.server().fileinfo(filename));
	int fd = ::open(fi->filename().c_str(), O_RDONLY);
	if (fi->exists() && fd >= 0)
	{
		responseHeaders.overwrite("Content-Type", fi->mimetype());
		responseHeaders.overwrite("Content-Length", boost::lexical_cast<std::string>(fi->size()));

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

		responseHeaders.overwrite("Content-Type", "text/html");
		responseHeaders.overwrite("Content-Length", boost::lexical_cast<std::string>(nwritten));

		return std::make_shared<BufferSource>(Buffer::from_copy(buf, nwritten));
	}
}

std::string HttpRequest::statusStr(HttpError value)
{
	return http_category().message(static_cast<int>(value));
}

/** completion handler, being invoked when this response has been fully flushed and is considered done.
 * \see HttpRequest::finish()
 */
void HttpRequest::onFinished(int ec)
{
	//TRACE("HttpRequest(%p).onFinished(%d)", this, ec);

	{
		HttpServer& srv = connection.worker().server();

		// log request/response
		srv.onRequestDone(this);
	}

	// close, if not a keep-alive connection
	if (iequals(responseHeaders["Connection"], "keep-alive"))
		connection.resume();
	else
		connection.close();
}

/** finishes this response by flushing the content into the stream.
 *
 * \note this also queues the underlying connection for processing the next request (on keep-alive).
 */
void HttpRequest::finish()
{
	if (!headersSent_) // nothing sent to client yet -> sent default status page
	{
		if (static_cast<int>(status) == 0)
			status = HttpError::NotFound;

		if (!responseContentForbidden() && status != HttpError::Ok)
			write(makeDefaultResponseContent(), std::bind(&HttpRequest::onFinished, this, std::placeholders::_1));
		else
			connection.writeAsync(serialize(), std::bind(&HttpRequest::onFinished, this, std::placeholders::_1));
	}
	else if (!outputFilters.empty())
	{
		// mark the end of stream (EOS) by passing an empty chunk to the outputFilters.
		connection.writeAsync(
			std::make_shared<FilterSource>(std::make_shared<BufferSource>(""), outputFilters, true),
			std::bind(&HttpRequest::onFinished, this, std::placeholders::_1)
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
void HttpRequest::initialize()
{
	// pre-compute string representations of status codes for use in response serialization
	for (std::size_t i = 0; i < sizeof(statusCodes_) / sizeof(*statusCodes_); ++i)
		snprintf(statusCodes_[i], sizeof(*statusCodes_), "%03ld", i);
}

} // namespace x0
