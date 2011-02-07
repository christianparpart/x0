/* <x0/Types.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
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

class FileInfo;
class HttpListener;
class HttpConnection;
class HttpPlugin;
struct HttpRequest;

struct File;

//! \addtogroup io
//@{

typedef std::shared_ptr<File> FilePtr;
typedef std::shared_ptr<FileInfo> FileInfoPtr;

//@}

//! \addtogroup io
//@{

typedef std::shared_ptr<HttpPlugin> HttpPluginPtr;

/** completion handler.
 *
 * used for handlers invoked when done writing or reading from a HttpConnection.
 */
typedef std::function<void(
	int /*errno*/, // const asio::error_code& /*ec*/,
	std::size_t /*bytes_transferred*/)
> CompletionHandlerType;

/** HttpRequest handler.
 */
typedef std::function<void(
	HttpRequest& /*in*/,
	const std::function<void()>& /*completionHandler*/)
> RequestHandlerType;

//@}

} // namespace x0

#endif
