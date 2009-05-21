/* <x0/main.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/handler.hpp>
#include <x0/server.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

void logger(x0::request& req, x0::response&) {
	std::cout << " - " << req.uri << std::endl;
}

int main(int argc, char *argv[])
{
	x0::io_service ios;
	x0::server server(ios);

	server.connection_logger.connect(&logger);

	server.start();

	ios.run();

	return 0;
}
