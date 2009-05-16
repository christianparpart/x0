#include <x0/server.hpp>
#include <x0/config.hpp>
#include <http/server.hpp>
#include <cstdlib>

namespace x0 {

server::server(boost::asio::io_service& io_service)
  : listeners_(),
	io_service_(io_service)
{
}

server::~server()
{
}

void server::start()
{
	configure();

	for (auto i = listeners_.begin(); i != listeners_.end(); ++i)
	{
		(*i)->start();
	}
}

void server::configure()
{
	config global;
	global.load_file("x0d.conf");

	// populate vhosts database
	config vhosts;
	vhosts.load_file(global.get("service", "vhosts-file"));

	for (auto i = vhosts.cbegin(); i != vhosts.cend(); ++i)
	{
		std::string hostname = i->first;
		int port = std::atoi(vhosts.get(hostname, "port").c_str());

		vhosts_[vhost_selector(hostname, port)].reset(new vhost(i->second));
	}

	// populate TCP servers
	for (auto i = vhosts_.begin(); i != vhosts_.end(); ++i)
	{
		std::string address = "0::0"; // bind address
		int port = i->first.port;

		// check if we already have an HTTP server listening on given port
		if (listener_by_port(port))
			continue;

		// create a new listener
		http::server_ptr http_server(new http::server(io_service_));

		http_server->configure(address, port);

		listeners_.insert(http_server);
	}
}

http::server_ptr server::listener_by_port(int port)
{
	for (auto k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		http::server_ptr http_server = *k;

		if (http_server->port() == port)
		{
			return http_server;
		}
	}

	return http::server_ptr();
}

void server::pause()
{
}

void server::resume()
{
}

void server::stop()
{
	for (auto k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		(*k)->stop();
	}
}

} // namespace x0
