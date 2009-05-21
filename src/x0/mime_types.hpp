/* <x0/mime_types.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_mime_types_hpp
#define x0_mime_types_hpp (1)

#include <string>

namespace x0 {
namespace mime_types {

std::string extension_to_type(const std::string& extension);

} // namespace mime_types
} // namespace x0

#endif
