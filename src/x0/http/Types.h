/* <x0/http/Types.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_http_types_hpp
#define sw_x0_http_types_hpp (1)

#include <x0/Types.h>
#include <x0/Api.h>

#include <system_error>

namespace x0 {

class SettingsValue;
class Scope;

typedef std::function<std::error_code(const SettingsValue&, Scope&)> cvar_handler;

} // namespace x0

#endif
