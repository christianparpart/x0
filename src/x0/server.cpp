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
//		(*i)->start();
	}
}

void server::configure()
{
	config global;
	global.load_file("x0d.conf");

	// populate vhosts database
	config vhosts;
	vhosts.load_file(global.get("service", "vhosts-file"));
	std::cout << vhosts.serialize();

	for (auto i = vhosts.cbegin(); i != vhosts.cend(); ++i)
	{
		std::string hostname = i->first;
		int port = std::atoi(vhosts.get(hostname, "port").c_str());

		std::cout << "vhost: " << hostname << " port " << port << std::endl;

		vhosts_[vhost_selector(hostname, port)].reset(new vhost(i->second));
	}
	// populate TCP servers
	for (auto i = vhosts_.begin(); i != vhosts_.end(); ++i)
	{
		int port = i->first.port;

		// check if we already have an HTTP server listening on given port
		for (auto k = listeners_.begin(); k != listeners_.end(); ++k)
		{
			http::server_ptr http_server = *k;

			if (http_server->port() == port)
			{
				continue;
			}
		}
#if 0
		// create a new listener
		listeners_.insert(http::server_ptr(new http::server(io_service_)));
#endif
	}
}

void server::pause()
{
}

void server::resume()
{
}

void server::stop()
{
}

} // namespace x0
