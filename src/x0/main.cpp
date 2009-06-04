/* <x0/main.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
	try
	{
		boost::asio::io_service ios;
		x0::server server(ios);

		server.start(argc, argv);

		ios.run();

		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
