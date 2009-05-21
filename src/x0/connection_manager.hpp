/* <x0/connection_manager.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_manager_hpp
#define x0_connection_manager_hpp (1)

#include <set>
#include <x0/connection.hpp>
#include <x0/types.hpp>

namespace x0 {

/**
 * manages open client connections.
 */
class connection_manager :
	private noncopyable
{
public:
	/// adds a new connection to the manager and start it.
	void start(connection_ptr connection);

	/// stops given connection.
	void stop(connection_ptr connection);

	/// stops all connections.
	void stop_all();

private:
	std::set<connection_ptr> connections_;
};

} // namespace x0 

#endif
