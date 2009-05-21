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

using namespace boost;
using namespace boost::asio;

class connection;

typedef shared_ptr<connection> connection_ptr;

} // namespace x0

#endif
