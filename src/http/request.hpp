#ifndef x0_http_request_hpp
#define x0_http_request_hpp (1)

#include <string>
#include <vector>
#include <http/header.hpp>

namespace http {

/// a client http reuqest object, holding the parsed http request data.
struct request
{
	std::string method;
	std::string uri;
	int http_version_major;
	int http_version_minor;
	std::vector<header> headers;
};

} // namespace http

#endif
