/* <src/capi.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/capi.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpRequest.h>

using namespace x0;

struct x0_server_s
{
	HttpServer server;
	ServerSocket* listener;

	x0_handler_t callback_handler;
	void* callback_userdata;

	x0_server_s(struct ev_loop* loop) :
		server(loop),
		listener(nullptr),
		callback_handler(nullptr),
		callback_userdata(nullptr)
	{}
};

struct x0_request_s
{
	x0_server_t* server;
	HttpRequest* request;
};

x0_server_t* x0_server_create(int port, const char* bind, struct ev_loop* loop)
{
	auto li = new x0_server_t(loop);
	return li;
}

void x0_server_destroy(x0_server_t* server, int kill)
{
	delete server;
}

void x0_setup_connection_limit(x0_server_t* li, size_t limit)
{
	li->server.maxConnections = limit;
}

void x0_setup_timeouts(x0_server_t* server, int read, int write)
{
	server->server.maxReadIdle = TimeSpan::fromSeconds(read);
	server->server.maxWriteIdle = TimeSpan::fromSeconds(write);
}

void x0_setup_keepalive(x0_server_t* server, int timeout, int count)
{
	server->server.maxKeepAlive = TimeSpan::fromSeconds(timeout);
	server->server.maxKeepAliveRequests = count;
}
