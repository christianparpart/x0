#include <iostream>
#include <string>
#include <x0/server.hpp>
#include <x0/config.hpp>
#include <http/server.hpp>
#include <boost/asio.hpp>

int main(int argc, char *argv[])
{
	boost::asio::io_service io_service;
#if 0
	http::server http_server(io_service);

	http_server.start("0::0", 8080);

	io_service.run();

	std::cout << "x0d: shutting down ..." << std::endl;
#else
	x0::server x0_server(io_service);

	x0_server.start();

	io_service.run();
#endif
	return 0;
}
