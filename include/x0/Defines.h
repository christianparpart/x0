/* <x0/Defines.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_defines_hpp
#define sw_x0_defines_hpp (1)

#include <stddef.h>
#include <string.h>
#include <stdio.h>

// platforms
#if defined(_WIN32) || defined(__WIN32__)
#	define X0_OS_WIN32 1
//#	define _WIN32_WINNT 0x0510
#else
#	define X0_OS_UNIX 1
#	if defined(__CYGWIN__)
#		define X0_OS_WIN32 1
#	elif defined(__APPLE__)
#		define X0_OS_DARWIN 1 /* MacOS/X 10 */
#	endif
#endif

// api decl tools
#if defined(__GNUC__)
#	define X0_NO_EXPORT __attribute__((visibility("hidden")))
#	define X0_EXPORT __attribute__((visibility("default")))
#	define X0_IMPORT /*!*/
#	define X0_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#	define X0_NO_RETURN __attribute__((no_return))
#	define X0_DEPRECATED __attribute__((__deprecated__))
#	define X0_PURE __attribute__((pure))
#	define X0_PACKED __attribute__((packed))
#	if !defined(likely)
#		define likely(x) __builtin_expect((x), 1)
#	endif
#	if !defined(unlikely)
#		define unlikely(x) __builtin_expect((x), 0)
#	endif
#elif defined(__MINGW32__)
#	define X0_NO_EXPORT /*!*/
#	define X0_EXPORT __declspec(export)
#	define X0_IMPORT __declspec(import)
#	define X0_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#	define X0_NO_RETURN __attribute__((no_return))
#	define X0_DEPRECATED __attribute__((__deprecated__))
#	define X0_PURE __attribute__((pure))
#	define X0_PACKED __attribute__((packed))
#	if !defined(likely)
#		define likely(x) (x)
#	endif
#	if !defined(unlikely)
#		define unlikely(x) (x)
#	endif
#elif defined(__MSVC__)
#	define X0_NO_EXPORT /*!*/
#	define X0_EXPORT __declspec(export)
#	define X0_IMPORT __declspec(import)
#	define X0_WARN_UNUSED_RESULT /*!*/
#	define X0_NO_RETURN /*!*/
#	define X0_DEPRECATED /*!*/
#	define X0_PURE /*!*/
#	define X0_PACKED __packed /* ? */
#	if !defined(likely)
#		define likely(x) (x)
#	endif
#	if !defined(unlikely)
#		define unlikely(x) (x)
#	endif
#else
#	warning Unknown platform
#	define X0_NO_EXPORT /*!*/
#	define X0_EXPORT /*!*/
#	define X0_IMPORT /*!*/
#	define X0_WARN_UNUSED_RESULT /*!*/
#	define X0_NO_RETURN /*!*/
#	define X0_DEPRECATED /*!*/
#	define X0_PURE /*!*/
#	define X0_PACKED /*!*/
#	if !defined(likely)
#		define likely(x) (x)
#	endif
#	if !defined(unlikely)
#		define unlikely(x) (x)
#	endif
#endif

#if defined(__GNUC__)
	#define GCC_VERSION(major, minor) ( \
		(__GNUC__ > (major)) || \
		(__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)) \
	)
	#define CC_SUPPORTS_LAMBDA GCC_VERSION(4, 5)
	#define CC_SUPPORTS_RVALUE_REFERENCES GCC_VERSION(4, 4)
#else
	#define GCC_VERSION(major, minor) (0)
	#define CC_SUPPORTS_LAMBDA (0)
	#define CC_SUPPORTS_RVALUE_REFERENCES (0)
#endif

/// the filename only part of __FILE__ (no leading path)
#define __FILENAME__ ((std::strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)

#if !defined(DEBUG)
// TODO remove me
#	define DEBUG(msg...) /*!*/
#endif

#endif
