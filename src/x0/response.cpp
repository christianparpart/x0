/* <x0/response.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/response.hpp>
#include <x0/server.hpp>
#include <x0/io/file.hpp>
#include <x0/io/file_source.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/io/chunked_encoder.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <x0/sysconfig.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <strings.h>						// strcasecmp
#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("response: " msg)
#endif

namespace x0 {

using boost::algorithm::iequals;

char response::status_codes[512][4];

response::~response()
{
	//TRACE("~response(%p, conn=%p)", this, connection_);
}

template<typename T>
inline std::string make_str(T value)
{
	return boost::lexical_cast<std::string>(value);
}

source_ptr response::make_default_content()
{
	if (content_forbidden()) // || !equals(request_->method, "GET"))
		return source_ptr();

	std::string filename(connection_->server().config()["ErrorDocuments"][make_str(status)].as<std::string>());
	fileinfo_ptr fi(connection_->server().fileinfo(filename));
	if (fi->exists())
	{
		file_ptr f(new file(fi));

		headers.set("Content-Type", fi->mimetype());
		headers.set("Content-Length", boost::lexical_cast<std::string>(fi->size()));

		return std::make_shared<file_source>(f);
	}
	else
	{
		const char *codeStr = status_cstr(status);
		char buf[1024];

		int nwritten = snprintf(buf, sizeof(buf),
			"<html>"
			"<head><title>%s</title></head>"
			"<body><h1>%d %s</h1></body>"
			"</html>\r\n",
			codeStr, status(), codeStr
		);

		headers.set("Content-Type", "text/html");
		headers.set("Content-Length", boost::lexical_cast<std::string>(nwritten));

		return std::make_shared<buffer_source>(buffer::from_copy(buf, nwritten));
	}
}

source_ptr response::serialize()
{
	buffer buffers;
	bool keepalive = false;

	if (!status)
	{
		status = response::ok;
	}

	if (!headers.contains("Content-Type"))
	{
		headers.push_back("Content-Type", "text/plain"); //!< \todo pass "default" content-type instead!
	}

	// post-response hook
	connection_->server().post_process(const_cast<x0::request *>(request_), this);

	// setup (connection-level) response transfer
	if (!headers.contains("Content-Length") && !content_forbidden())
	{
		if (request_->supports_protocol(1, 1)
			&& !headers.contains("Transfer-Encoding")
			&& !content_forbidden())
		{
			headers.push_back("Transfer-Encoding", "chunked");
			filter_chain.push_back(std::make_shared<chunked_encoder>());
			keepalive = true;
		}
		else
		{
			headers.set("Connection", "close");
		}
	}
	else if (!headers.contains("Connection"))
	{
		if (iequals(request_->header("Connection"), "keep-alive"))
		{
			headers.push_back("Connection", "keep-alive");
			keepalive = true;
		}
		else
			headers.push_back("Connection", "close");
	}
	else if (iequals(headers("Connection"), "keep-alive"))
	{
		keepalive = true;
	}

#if defined(TCP_CORK)
	if (!keepalive && connection_->server().tcp_cork())
	{
		TRACE("enabling TCP_CORK");
		int flag = 1;
		setsockopt(connection_->handle(), IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
	}
#endif

	if (request_->supports_protocol(1, 1))
		buffers.push_back("HTTP/1.1 ");
	else if (request_->supports_protocol(1, 0))
		buffers.push_back("HTTP/1.0 ");
	else
		buffers.push_back("HTTP/0.9 ");

	buffers.push_back(status_codes[status]);
	buffers.push_back(' ');
	buffers.push_back(status_cstr(status));
	buffers.push_back("\r\n");

	for (auto i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		const response_header& h = *i;
		buffers.push_back(h.name.data(), h.name.size());
		buffers.push_back(": ");
		buffers.push_back(h.value.data(), h.value.size());
		buffers.push_back("\r\n");
	};

	buffers.push_back("\r\n");

	return std::make_shared<buffer_source>(std::move(buffers));
}

response::response(connection *connection, x0::request *request, int _status) :
	connection_(connection),
	request_(request),
	headers_sent_(false),
	status(_status),
	headers()
{
	connection_->response_ = this;

	//TRACE("response(%p, conn=%p)", this, connection_);

	headers.push_back("Date", connection_->server().now().http_str().str());

	if (connection_->server().advertise() && !connection_->server().tag().empty())
		headers.push_back("Server", connection_->server().tag());
}

const char *response::status_cstr(int value)
{
	switch (value)
	{
		// information
		case 100: return "Continue";
		case 101: return "Switching Protocols";
		case 102: return "Processing";

		// success
		case 200: return "Ok";
		case 201: return "Created";
		case 202: return "Accepted";
		case 204: return "No Content";
		case 206: return "Partial Content";

		// redirect
		case 300: return "Multiple Choices";
		case 301: return "Moved Permanently";
		case 302: return "Moved Temporarily";
		case 304: return "Not Modified";

		// client errors
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 406: return "Not Acceptable";
		case 407: return "Proxy Authentication Required";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 410: return "Gone";
		case 411: return "Length Required";
		case 412: return "Precondition Failed";
		case 413: return "Request Entity Too Large";
		case 414: return "Request URI Too Long";
		case 415: return "Unsupported Media Type";
		case 416: return "Requested Range Not Satisfiable";
		case 417: return "Expectaction Failed";
		case 421: return "There Are Too Many Connections From Your Internet Address";
		case 422: return "Unprocessable Entity";
		case 423: return "Locked";
		case 424: return "Failed Dependency";
		case 425: return "Unordered Collection";
		case 426: return "Upgrade Required";

		// server errors
		case 500: return "Internal Server Error";
		case 501: return "Not_Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timed Out";
		case 505: return "HTTP Version Not Supported";
		case 507: return "Insufficient Storage";
		case 509: return "Bandwidth Limit Exceeded";
		case 510: return "Not Extended";

		// unknown
		default: return "";
	}
}

std::string response::status_str(int value)
{
	return std::string(status_cstr(value));
}

void response::finished0(int ec)
{
	//TRACE("response(%p).finished(%d)", this, ec);

	if (filter_chain.empty())
		finished1(ec);
	else
		connection_->async_write(std::make_shared<filter_source>(filter_chain),
			std::bind(&response::finished1, this, std::placeholders::_1));
}

/** handler, being invoked when this response has been fully flushed and is considered done.
 */
void response::finished1(int ec)
{
	//TRACE("response(%p).finished_next(%d)", this, ec);

	{
		server& srv = request_->connection.server();

		// log request/response
		srv.request_done(const_cast<x0::request *>(request_), this);
	}

	if (strcasecmp(headers["Connection"].c_str(), "keep-alive") == 0)
		connection_->resume();
	else
		delete connection_;
}

void response::initialize()
{
	// pre-compute string representations of status codes for use in response serialization
	for (std::size_t i = 0; i < sizeof(status_codes) / sizeof(*status_codes); ++i)
		snprintf(status_codes[i], sizeof(*status_codes), "%03ld", i);
}

} // namespace x0
