/*! \brief Very simple HTTP server. Everything's done for you.
 *
 */
#include <iostream>
#include <x0/http/HttpServer.h>

int main(int argc, const char *argv[])
{
	x0::HttpServer srv;

	std::clog << "Loading x0 configuration" << std::endl;
	srv.configure("app1.conf");

	std::clog << "Running HTTP server" << std::endl;
	srv.run();

	return 0;
}
