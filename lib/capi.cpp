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

struct x0_listener_s
{
	HttpServer server;
	ServerSocket* listener;

	x0_handler_t callback_handler;
	void* callback_userdata;

	x0_listener_s(struct ev_loop* loop) :
		server(loop),
		listener(nullptr),
		callback_handler(nullptr),
		callback_userdata(nullptr)
	{}
};

struct x0_request_s
{
	x0_listener_t* listener;
	HttpRequest* request;
};

x0_listener_t* x0_listener_create(int port, const char* bind, struct ev_loop* loop)
{
	auto li = new x0_listener_t(loop);
	return li;
}

void x0_listener_destroy(x0_listener_t* listener)
{
	delete listener;
}
