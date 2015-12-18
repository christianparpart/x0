// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef sw_x0_defines_hpp
#define sw_x0_defines_hpp (1)

#include <xzero/sysconfig.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// platforms
#if defined(_WIN32) || defined(__WIN32__)
#define XZERO_OS_WIN32 1
//#	define _WIN32_WINNT 0x0510
#else
#define XZERO_OS_UNIX 1
#if defined(__CYGWIN__)
#define XZERO_OS_WIN32 1
#elif defined(__APPLE__)
#define XZERO_OS_DARWIN 1 /* MacOS/X 10 */
#elif defined(__linux__)
#define XZERO_OS_LINUX 1
#endif
#endif

// api decl tools
#if defined(__GNUC__)
#define XZERO_NO_EXPORT __attribute__((visibility("hidden")))
#define XZERO_EXPORT __attribute__((visibility("default")))
#define XZERO_IMPORT /*!*/
#define XZERO_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define XZERO_NO_RETURN __attribute__((no_return))
#define XZERO_DEPRECATED __attribute__((__deprecated__))
#define XZERO_PURE __attribute__((pure))
#define XZERO_PACKED __attribute__((packed))
#define XZERO_INIT __attribute__((constructor))
#define XZERO_FINI __attribute__((destructor))
#if !defined(likely)
#define likely(x) __builtin_expect((x), 1)
#endif
#if !defined(unlikely)
#define unlikely(x) __builtin_expect((x), 0)
#endif
#elif defined(__MINGW32__)
#define XZERO_NO_EXPORT /*!*/
#define XZERO_EXPORT __declspec(export)
#define XZERO_IMPORT __declspec(import)
#define XZERO_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define XZERO_NO_RETURN __attribute__((no_return))
#define XZERO_DEPRECATED __attribute__((__deprecated__))
#define XZERO_PURE __attribute__((pure))
#define XZERO_PACKED __attribute__((packed))
#define XZERO_INIT /*!*/
#define XZERO_FINI /*!*/
#if !defined(likely)
#define likely(x) (x)
#endif
#if !defined(unlikely)
#define unlikely(x) (x)
#endif
#elif defined(__MSVC__)
#define XZERO_NO_EXPORT /*!*/
#define XZERO_EXPORT __declspec(export)
#define XZERO_IMPORT __declspec(import)
#define XZERO_WARN_UNUSED_RESULT /*!*/
#define XZERO_NO_RETURN          /*!*/
#define XZERO_DEPRECATED         /*!*/
#define XZERO_PURE               /*!*/
#define XZERO_PACKED __packed    /* ? */
#define XZERO_INIT               /*!*/
#define XZERO_FINI               /*!*/
#if !defined(likely)
#define likely(x) (x)
#endif
#if !defined(unlikely)
#define unlikely(x) (x)
#endif
#else
#warning Unknown platform
#define XZERO_NO_EXPORT          /*!*/
#define XZERO_EXPORT             /*!*/
#define XZERO_IMPORT             /*!*/
#define XZERO_WARN_UNUSED_RESULT /*!*/
#define XZERO_NO_RETURN          /*!*/
#define XZERO_DEPRECATED         /*!*/
#define XZERO_PURE               /*!*/
#define XZERO_PACKED             /*!*/
#define XZERO_INIT               /*!*/
#define XZERO_FINI               /*!*/
#if !defined(likely)
#define likely(x) (x)
#endif
#if !defined(unlikely)
#define unlikely(x) (x)
#endif
#endif

#if defined(__GNUC__)
#define GCC_VERSION(major, minor) \
  ((__GNUC__ > (major)) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
#define CC_SUPPORTS_LAMBDA GCC_VERSION(4, 5)
#define CC_SUPPORTS_RVALUE_REFERENCES GCC_VERSION(4, 4)
#else
#define GCC_VERSION(major, minor) (0)
#define CC_SUPPORTS_LAMBDA (0)
#define CC_SUPPORTS_RVALUE_REFERENCES (0)
#endif

/// the filename only part of __FILE__ (no leading path)
#define __FILENAME__ ((std::strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)

#if defined(XZERO_ENABLE_NOEXCEPT)
#define XZERO_NOEXCEPT noexcept
#else
#define XZERO_NOEXCEPT /*XZERO_NOEXCEPT*/
#endif

#endif
