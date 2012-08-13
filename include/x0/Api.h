/* <x0/Api.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_api_hpp
#define sw_x0_api_hpp (1)

#include <x0/Defines.h>

// x0 exports
#if defined(BUILD_X0)
#	define X0_API X0_EXPORT
#else
#	define X0_API X0_IMPORT
#endif

#endif
