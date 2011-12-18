/* <x0/HttpRequest.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpRequest.h>
#include <x0/http/HttpConnection.h>
#include <x0/io/BufferSource.h>
#include <x0/io/FilterSource.h>
#include <x0/io/ChunkedEncoder.h>
#include <x0/Process.h>                // Process::dumpCore()
#include <x0/strutils.h>
#include <strings.h>                   // strcasecmp()

#if !defined(NDEBUG)
#	define TRACE(msg...) this->debug(msg)
#else
#	define TRACE(msg...) do { } while (0)
#endif

namespace x0 {

char HttpRequest::statusCodes_[512][4];

HttpRequest::HttpRequest(HttpConnection& conn) :
	outputState_(Unhandled),
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
	bytesTransmitted_(0),
	username(),
	documentRoot(),
	expectingContinue(false),
	//customData(),

	status(),
	responseHeaders(),
	outputFilters(),

	inspectHandlers_(),

	hostid_(),
	bodyCallback_(nullptr),
	bodyCallbackData_(nullptr),
	errorHandler_(nullptr)
{
#ifndef NDEBUG
	static std::atomic<unsigned long long> rid(0);
	setLoggingPrefix("HttpRequest(%lld,%s:%d)", ++rid, connection.remoteIP().c_str(), connection.remotePort());
#endif

	responseHeaders.push_back("Date", connection.worker().now().http_str().str());

	if (connection.worker().server().advertise() && !connection.worker().server().tag().empty())
		if (!responseHeaders.contains("Server"))
			responseHeaders.push_back("Server", connection.worker().server().tag());
}

HttpRequest::~HttpRequest()
{
	TRACE("destructing");
}

void HttpRequest::updatePathInfo()
{
	if (!fileinfo)
		return;

	// split "/the/tail" from "/path/to/script.php/the/tail"

	std::string fullname(fileinfo->path());
	size_t origpos = fullname.size() - 1, pos = origpos;

	for (;;) {
		if (fileinfo->exists()) {
			if (pos != origpos)
				pathinfo = fullname.substr(pos);

			break;
		} if (fileinfo->error() == ENOTDIR) {
			pos = fileinfo->path().rfind('/', pos - 1);
			fileinfo = connection.worker().fileinfo(fileinfo->path().substr(0, pos));
		} else {
			break;
		}
	}
}

BufferRef HttpRequest::requestHeader(const std::string& name) const
{
	for (auto& i: requestHeaders)
		if (iequals(i.name, name))
			return i.value;

	return BufferRef();
}

std::string HttpRequest::hostid() const
{
	if (hostid_.empty())
		hostid_ = x0::make_hostid(hostname, connection.listener().port());

	return hostid_;
}

void HttpRequest::setHostid(const std::string& value)
{
	hostid_ = value;
}

/** Reports true whenever content is still in queue to be read (even if not yet received).
 *
 * @retval true there is still more content in the queue to be processed.
 * @retval false no content expected anymore, thus, request fully parsed.
 */
bool HttpRequest::contentAvailable() const
{
	return connection.contentLength() > 0;
	//return connection.state() != HttpMessageProcessor::MESSAGE_BEGIN;
}

/*! setup request-body consumer callback.
 *
 * \note This function must be invoked within the request handler before it passes control back to the caller.
 * \warning Only register a callback, if you're to serve the request's reply.
 *
 * \param callback the callback to invoke on request-body chunks.
 * \param data a custom data pointer being also passed to the callback.
 */
void HttpRequest::setBodyCallback(void (*callback)(const BufferRef&, void*), void* data)
{
	bodyCallback_ = callback;
	bodyCallbackData_ = data;

	if (expectingContinue) {
		connection.write<BufferSource>("HTTP/1.1 100 Continue\r\n\r\n");
		expectingContinue = false;
	}
}

/** Passes the request body chunk to the applications registered callback.
 *
 * \see setBodyCallback()
 */
void HttpRequest::onRequestContent(const BufferRef& chunk)
{
	if (bodyCallback_) {
		TRACE("onRequestContent(chunkSize=%ld) pass to callback", chunk.size());
		bodyCallback_(chunk, bodyCallbackData_);
	} else {
		TRACE("onRequestContent(chunkSize=%ld) discard", chunk.size());
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
Source* HttpRequest::serialize()
{
	Buffer buffers;

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
	if (supportsProtocol(1, 1)
		&& !responseHeaders.contains("Content-Length")
		&& !responseHeaders.contains("Transfer-Encoding")
		&& !isResponseContentForbidden())
	{
		responseHeaders.push_back("Transfer-Encoding", "chunked");
		outputFilters.push_back(std::make_shared<ChunkedEncoder>());
	}

	bool keepalive = connection.shouldKeepAlive();
	if (!connection.worker().server().maxKeepAlive())
		keepalive = false;

	// remaining request count, that is allowed on a persistent connection
	std::size_t rlim = connection.worker().server().maxKeepAliveRequests();
	if (rlim) {
		rlim = connection.requestCount_ <= rlim
			? rlim - connection.requestCount_ + 1
			: 0;

		if (rlim == 0)
			// disable keep-alive, if max request count has been reached
			keepalive = false;
	}

	// only set Connection-response-header if found as request-header, too
	if (!requestHeader("Connection").empty() || keepalive != connection.shouldKeepAlive()) {
		if (keepalive) {
			responseHeaders.overwrite("Connection", "keep-alive");

			if (rlim) {
				// sent keep-alive timeout and remaining request count
				char buf[80];
				snprintf(buf, sizeof(buf), "timeout=%ld, max=%ld",
					static_cast<time_t>(connection.worker().server().maxKeepAlive().value()), rlim);
				responseHeaders.overwrite("Keep-Alive", buf);
			} else {
				// sent keep-alive timeout only (infinite request count)
				char buf[80];
				snprintf(buf, sizeof(buf), "timeout=%ld",
					static_cast<time_t>(connection.worker().server().maxKeepAlive().value()));
				responseHeaders.overwrite("Keep-Alive", buf);
			}
		} else
			responseHeaders.overwrite("Connection", "close");
	}

	connection.setShouldKeepAlive(keepalive);

	if (!connection.worker().server().tcpCork())
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

	for (auto& i: responseHeaders) {
		buffers.push_back(i.name.data(), i.name.size());
		buffers.push_back(": ");
		buffers.push_back(i.value.data(), i.value.size());
		buffers.push_back("\r\n");
	};

	buffers.push_back("\r\n");

	return new BufferSource(std::move(buffers));
}

/** populates a default-response content, possibly modifying a few response headers, too.
 *
 * \return the newly created response content or a null ptr if the statuc code forbids content.
 *
 * \note Modified headers are "Content-Type" and "Content-Length".
 */
void HttpRequest::writeDefaultResponseContent()
{
	if (isResponseContentForbidden())
		return;

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

	char slen[64];
	snprintf(slen, sizeof(slen), "%d", nwritten);
	responseHeaders.overwrite("Content-Length", slen);

	write<BufferSource>(Buffer::fromCopy(buf, nwritten));
}

std::string HttpRequest::statusStr(HttpError value)
{
	return http_category().message(static_cast<int>(value));
}

/** Finishes handling the current request.
 *
 * This results either in a closed underlying connection or the connection waiting for the next request to arrive,
 * or, if pipelined, processing the next request right away.
 *
 * If finish() is invoked with no response being generated yet, it will be generate a default response automatically,
 * depending on the \b status code set.
 *
 * \p finish() can be invoked even if there is still output pending, but it may not be added more output to the connection
 * by the application after \e finish() has been invoked.
 *
 * \note We might also trigger the custom error page handler, if no content was given.
 * \note This also queues the underlying connection for processing the next request (on keep-alive).
 * \note This also clears out the client abort callback, as set with \p setAbortHandler().
 * \note This also clears the body content chunk callback, as set with \p setBodyCallback().
 *
 * \see HttpConnection::write(), HttpConnection::isOutputPending()
 * \see finalize()
 */
void HttpRequest::finish()
{
	TRACE("finish(%d)", outputState_);

	setAbortHandler(nullptr);
	setBodyCallback(nullptr);

	if (isAborted()) {
		outputState_ = Finished;
		finalize();
		return;
	}

	switch (outputState_) {
		case Unhandled:
			if (static_cast<int>(status) == 0)
				status = HttpError::NotFound;

			if (errorHandler_) {
				TRACE("running custom error handler");
				// reset the handler right away to avoid endless nesting
				auto handler = errorHandler_;
				errorHandler_ = nullptr;

				if (handler(this))
					return;

				// the handler did not produce any response, so default to the
				// buildin-output
			}
			TRACE("streaming default error content");

			if (isResponseContentForbidden()) {
				connection.write(serialize());
			} else if (status == HttpError::Ok) {
				responseHeaders.overwrite("Content-Length", "0");
				connection.write(serialize());
			} else {
				writeDefaultResponseContent();
			}
			/* fall through */
		case Populating:
			// FIXME: can it become an issue when the response body may not be non-empty
			// but we have outputFilters defined, thus, writing a single (possibly empty?)
			// response body chunk?
			if (!outputFilters.empty()) {
				// mark the end of stream (EOS) by passing an empty chunk to the outputFilters.
				connection.write<FilterSource>(&outputFilters, true);
			}

			outputState_ = Finished;

			if (!connection.isOutputPending()) {
				// the response body is already fully transmitted, so finalize this request object directly.
				finalize();
			}
			break;
		case Finished:
#if !defined(NDEBUG)
			log(Severity::error, "BUG: invalid invocation of finish() on a already finished request.");
			Process::dumpCore();
#endif
			break;
	}
}

/** internally invoked when the response has been <b>fully</b> flushed to the client and we're to pass control back to the underlying connection.
 *
 * The request's reply has been fully transmitted to the client, so we are to invoke the request-done callback
 * and clear request-local custom data.
 *
 * After that, the connection gets either \p close()'d or will enter \p resume() state, to receive and process the next request.
 *
 * \see finish()
 */
void HttpRequest::finalize()
{
	TRACE("finalize()");
	connection.worker().server().onRequestDone(this);
	clearCustomData();

	if (isAborted() || !connection.shouldKeepAlive()) {
		TRACE("finalize: closing");
		connection.close();
	} else {
		TRACE("finalize: resuming");
		clear();
		connection.resume();
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

/** sets a callback to invoke on early connection aborts (by the remote end).
 *
 * This callback is only invoked when the client closed the connection before
 * \p HttpRequest::finish() has been invoked and completed already.
 *
 * Do <b>NOT</b> try to access the request object within the callback as it
 * might be destroyed already.
 *
 * \see HttpRequest::finish()
 */
void HttpRequest::setAbortHandler(void (*cb)(void *), void *data)
{
	connection.abortHandler_ = cb;
	connection.abortData_ = data;

	if (cb) {
		connection.watchInput();
	}
}

} // namespace x0
