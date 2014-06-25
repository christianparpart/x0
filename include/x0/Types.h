// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
