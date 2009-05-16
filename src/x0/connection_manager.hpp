#ifndef x0_connection_manager_hpp
#define x0_connection_manager_hpp (1)

#include <set>
#include <x0/connection.hpp>

namespace x0 {

/**
 * manages open client connections.
 */
class connection_manager :
	private boost::noncopyable
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
