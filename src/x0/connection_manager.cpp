/* <x0/connection_manager.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/connection_manager.hpp>
#include <boost/bind.hpp>
#include <algorithm>

namespace x0 {

void connection_manager::start(connection_ptr connection)
{
	connections_.insert(connection);
	connection->start();
}

void connection_manager::stop(connection_ptr connection)
{
	connections_.erase(connection);
	connection->stop();
}

void connection_manager::stop_all()
{
	std::for_each(connections_.begin(), connections_.end(), bind(&connection::stop, _1));
	connections_.clear();
}

} // namespace x0
