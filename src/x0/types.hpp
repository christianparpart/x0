/* <x0/types.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009-2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_types_hpp
#define x0_types_hpp (1)

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
class plugin;
struct request;
struct response;

struct file;

//! \addtogroup core
//@{

typedef std::shared_ptr<file> file_ptr;
typedef std::shared_ptr<fileinfo> fileinfo_ptr;

typedef std::shared_ptr<plugin> plugin_ptr;

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

class custom_data
{
private:
	custom_data(const custom_data&) = delete;
	custom_data& operator=(const custom_data&) = delete;

public:
	custom_data() = default;
	virtual ~custom_data() {}
};

typedef std::shared_ptr<custom_data> custom_data_ptr;

//@}

} // namespace x0

#endif
