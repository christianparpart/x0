#include <x0/server.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
	boost::asio::io_service io_service;
	x0::server x0_server(io_service);

	x0_server.start();

	io_service.run();

	return 0;
}
