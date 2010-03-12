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
#include <x0/io/chunked_filter.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <strings.h>						// strcasecmp
#include <string>
#include <algorithm>

namespace x0 {

using boost::algorithm::iequals;

char response::status_codes[512][4];

response::~response()
{
	//DEBUG("~response(%p, conn=%p)", this, connection_);
	delete request_;

	if (strcasecmp(headers["Connection"].c_str(), "keep-alive") == 0)
		connection_->resume();
	else
		delete connection_;
}

/** checks wether given code MUST NOT have a response body. */
static inline bool content_forbidden(int code)
{
	switch (code)
	{
		case response::continue_:
		case response::switching_protocols:
		case response::no_content:
		case response::reset_content:
		case response::not_modified:
			return true;
		default:
			return false;
	}
}

template<typename T>
inline std::string make_str(T value)
{
	return boost::lexical_cast<std::string>(value);
}

source_ptr response::make_default_content()
{
	if (content_forbidden(status)) // || !equals(request_->method, "GET"))
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
			"</html>",
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

	if (!status)
	{
		status = response::ok;
	}

	if (!headers.contains("Content-Type"))
	{
		headers.push_back("Content-Type", "text/plain"); //!< \todo pass "default" content-type instead!
	}

	if (!headers.contains("Content-Length") && !content_forbidden(status))
	{
		headers.set("Connection", "closed");
	}
	else if (!headers.contains("Connection"))
	{
		if (iequals(request_->header("Connection"), "keep-alive"))
			headers.push_back("Connection", "keep-alive");
		else
			headers.push_back("Connection", "closed");
	}

	// post-response hook
	connection_->server().post_process(const_cast<x0::request *>(request_), this);

	// enable chunked transfer encoding if required and possible
	if (!headers.contains("Content-Length"))
	{
		if (request_->supports_protocol(1, 1)
			&& !headers.contains("Transfer-Encoding"))
		{
			headers.push_back("Transfer-Encoding", "chunked");
			filter_chain.push_back(std::make_shared<chunked_filter>());
		}
		else
		{
			headers.set("Connection", "closed");
		}
	}

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
	//DEBUG("response(%p, conn=%p)", this, connection_);

	headers.push_back("Date", connection_->server().now().http_str().str());
	headers.push_back("Server", connection_->server().tag());
}

const char *response::status_cstr(int value)
{
	switch (value)
	{
		case 200: return "Ok";
		case 201: return "Created";
		case 202: return "Accepted";
		case 204: return "No Content";
		case 206: return "Partial Content";
		case 300: return "Multiple Choices";
		case 301: return "Moved Permanently";
		case 302: return "Moved Temporarily";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 416: return "Requested Range Not Satisfiable";
		case 500: return "Internal Server Error";
		case 501: return "Not_Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		default: return "";
	}
}

std::string response::status_str(int value)
{
	return std::string(status_cstr(value));
}

/** handler, being invoked when this response has been fully flushed and is considered done.
 */
void response::finished(int ec)
{
	//DEBUG("response(%p).finished(%d)", this, ec);

	{
		server& srv = request_->connection.server();

		// log request/response
		srv.request_done(const_cast<x0::request *>(request_), this);
	}

	delete this;
}

void response::initialize()
{
	// pre-compute string representations of status codes for use in response serialization
	for (std::size_t i = 0; i < sizeof(status_codes) / sizeof(*status_codes); ++i)
		snprintf(status_codes[i], sizeof(*status_codes), "%03ld", i);
}

} // namespace x0
