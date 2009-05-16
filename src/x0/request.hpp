#ifndef x0_http_request_hpp
#define x0_http_request_hpp (1)

#include <string>
#include <vector>
#include <x0/header.hpp>

namespace x0 {

/// a client HTTP reuqest object, holding the parsed x0 request data.
struct request
{
	std::string method;
	std::string uri;
	int http_version_major;
	int http_version_minor;
	std::vector<header> headers;
};

} // namespace x0

#endif
