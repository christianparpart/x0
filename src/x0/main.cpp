/* <x0/main.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

class x0d
{
public:
	x0d() : ios_(), server_(ios_)
	{
	}

	~x0d()
	{
	}

	int run(int argc, char *argv[])
	{
		// TODO move cmdline parsing here (so that x0::server is lib-only)
		server_.start(argc, argv);
		ios_.run();
		return 0;
	}

private:
	boost::asio::io_service ios_;
	x0::server server_;
};

int main(int argc, char *argv[])
{
	try
	{
		x0d daemon;
		return daemon.run(argc, argv);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
