/* <x0/Url.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#if !defined(sw_x0_url_hpp)
#define sw_x0_url_hpp (1)

#include <x0/Api.h>
#include <string>

namespace x0 {

X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path, std::string& query);
X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path);
X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port);

} // namespace x0

#endif
