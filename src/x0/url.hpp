/* <x0/url.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009-2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#if !defined(sw_x0_url_hpp)
#define sw_x0_url_hpp (1)

#include <string>

namespace x0 {

bool parse_url(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path, std::string& query);
bool parse_url(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path);
bool parse_url(const std::string& spec, std::string& protocol, std::string& hostname, int& port);

} // namespace x0

#endif
