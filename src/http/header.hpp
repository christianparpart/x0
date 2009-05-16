#ifndef x0_http_header_hpp
#define x0_http_header_hpp (1)

#include <string>

namespace http {

// represents an HTTP header (name/value pair)
struct header {
	std::string name;
	std::string value;
};

} // namespace http

#endif
