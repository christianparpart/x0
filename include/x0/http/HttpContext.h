/* <x0/HttpContext.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_http_context_h
#define sw_x0_http_context_h (1)

#include <x0/Api.h>

namespace x0 {

enum class HttpContext
{
	SERVER		= 0x0001,
	HOST		= 0x0002,
	LOCATION	= 0x0004,
	DIRECTORY	= 0x0008,

	server		= SERVER,
	host		= HOST,
	location	= LOCATION,
	directory	= DIRECTORY
};


HttpContext operator|(HttpContext a, HttpContext b);
bool operator&(HttpContext a, HttpContext b);

// {{{ inlines
inline HttpContext operator|(HttpContext a, HttpContext b)
{
	return static_cast<HttpContext>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool operator&(HttpContext a, HttpContext b)
{
	return static_cast<int>(a) & static_cast<int>(b);
}
// }}}

} // namespace x0

#endif
