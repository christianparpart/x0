/* <src/x0d.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroDaemon.h>
#include <x0/DebugLogger.h>

int main(int argc, char *argv[])
{
	x0::DebugLogger::get().configure("XZERO_DEBUG");
	x0d::XzeroDaemon daemon(argc, argv);
	return daemon.run();
}
