/* <x0/types.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_types_h
#define x0_types_h (1)

#include <x0/api.hpp>
#include <memory>
#include <functional>
#include <cstring> // strrchr

namespace x0 {

/**
 * @defgroup base
 * @brief common base APIs that are x0 neutral but developed within and for the x0 web server and framework.
 */

/**
 * @defgroup core
 * @brief x0 web server core API.
 */

/**
 * @defgroup plugins
 * @brief x0 web server plugins.
 */

class fileinfo;
class listener;
class connection;
struct request;
struct response;

struct file;

//! \addtogroup core
//@{

typedef std::shared_ptr<file> file_ptr;
typedef std::shared_ptr<fileinfo> fileinfo_ptr;

/** completion handler.
 *
 * used for handlers invoked when done writing or reading from a connection.
 */
typedef std::function<void(
	int /*errno*/, // const asio::error_code& /*ec*/,
	std::size_t /*bytes_transferred*/)
> completion_handler_type;

/** request handler.
 */
typedef std::function<void(
	request& /*in*/,
	response& /*out*/,
	const std::function<void()>& /*completionHandler*/)
> request_handler_fn;

//@}

} // namespace x0

#endif
