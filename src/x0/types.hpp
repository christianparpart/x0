/* <x0/types.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_types_h
#define x0_types_h (1)

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <cstring> // strrchr

namespace x0 {

/**
 * @defgroup common
 * @brief common APIs that are x0 neutral but developed within and for the x0 web server.
 */

/**
 * @defgroup core
 * @brief x0 web server core API.
 */

/**
 * @defgroup modules
 * @brief x0 web server modules.
 */

class connection;
struct request;
struct response;

typedef boost::shared_ptr<connection> connection_ptr;
typedef boost::shared_ptr<response> response_ptr;

/**
 * \ingroup core
 * \brief request handler functor.
 */
typedef boost::function<void(request&, response&)> request_handler_fn;

#define __FILENAME__ ((std::strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)

} // namespace x0

#endif
