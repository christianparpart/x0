#ifndef x0_header_hpp
#define x0_header_hpp (1)

#include <string>

namespace x0 {

// represents an HTTP header (name/value pair)
struct header {
	std::string name;
	std::string value;
};

} // namespace x0

#endif
