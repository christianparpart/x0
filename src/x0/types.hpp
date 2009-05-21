/* <x0/types.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_defs_h
#define x0_defs_h (1)

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

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

using namespace boost;
using namespace boost::asio;

class connection;
struct response;

typedef shared_ptr<connection> connection_ptr;
typedef shared_ptr<response> response_ptr;

} // namespace x0

#endif
