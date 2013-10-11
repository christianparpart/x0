/* <x0/Api.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
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

#if defined(__cplusplus) && defined(__APPLE__)
// XXX WORKAROUND FOR APPLE OS X
namespace std {
        template<typename T> const T& move(const T& value) { return value; }
        template<typename T> const T& forward(const T& value) { return value; }
        template<typename> struct hash;
}
#endif

#endif
