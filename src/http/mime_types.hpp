#ifndef x0_http_mime_types_hpp
#define x0_http_mime_types_hpp (1)

#include <string>

namespace http {
namespace mime_types {

std::string extension_to_type(const std::string& extension);

} // namespace mime_types
} // namespace http

#endif
