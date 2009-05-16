#include <http/connection_manager.hpp>
#include <boost/bind.hpp>
#include <algorithm>

namespace http {

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
	std::for_each(connections_.begin(), connections_.end(), boost::bind(&connection::stop, _1));
	connections_.clear();
}

} // namespace http
