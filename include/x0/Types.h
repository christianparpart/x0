/* <x0/Types.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_Types_h
#define x0_Types_h (1)

#include <x0/Api.h>

#include <functional>
#include <memory>

namespace x0 {

/**
 * @defgroup base
 * @brief common base APIs that are x0 neutral but developed within and for the x0 web server and framework.
 */

/**
 * @defgroup io
 * @brief I/O streaming API (sources, filters, sinks)
 */

/**
 * @defgroup http
 * @brief HTTP web server APIs (like HttpRequest, HttpConnection, HttpServer, ...)
 */

/**
 * @defgroup sql
 * @brief module for managing SQL connections, results and prepared statements to mySQL.
 *
 * This is an abstract high-level interface to mySQL.
 */

/**
 * @defgroup plugins
 * @brief x0 web server plugins.
 */

} // namespace x0

#endif
