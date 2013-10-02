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
#include <x0/io/BufferSource.h>

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
	auto s = new x0_server_t(loop);

	s->listener = s->server.setupListener(bind, port, 128);

	s->server.requestHandler = [](HttpRequest* r) -> bool {
		BufferSource body("Hello, I am lazy to serve anything; I wasn't configured properly\n");

		r->status = HttpStatus::InternalServerError;

		char buf[40];
		snprintf(buf, sizeof(buf), "%zu", body.size());
		r->responseHeaders.push_back("Content-Length", buf);

		r->responseHeaders.push_back("Content-Type", "plain/text");
		r->write<BufferSource>(body);

		r->finish();

		return true;
	};

	return s;
}

void x0_server_destroy(x0_server_t* server, int kill)
{
	if (!kill) {
		// TODO wait until all current connections have been finished, if more than one worker has been set up
	}
	delete server;
}

void x0_server_stop(x0_server_t* server)
{
	server->server.stop();
}

void x0_setup_handler(x0_server_t* server, x0_handler_t handler, void* userdata)
{
	server->server.requestHandler = [=](HttpRequest* r) -> bool {
		auto rr = new x0_request_t;
		rr->request = r;
		rr->server = server;
		handler(rr, userdata);
		return true;
	};
}

void x0_response_status_set(x0_request_t* r, int code)
{
	r->request->status = static_cast<HttpStatus>(code);
}

void x0_response_header_set(x0_request_t* r, const char* name, const char* value)
{
	r->request->responseHeaders.overwrite(name, value);
}

void x0_response_finish(x0_request_t* r)
{
	r->request->finish();
	delete r;
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

void x0_setup_keepalive(x0_server_t* server, int count, int timeout)
{
	server->server.maxKeepAliveRequests = count;
	server->server.maxKeepAlive = TimeSpan::fromSeconds(timeout);
}

size_t x0_request_path(x0_request_t* r, char* buf, size_t size)
{
	size_t rsize = std::min(size - 1, r->request->path.size());
	memcpy(buf, r->request->path.data(), rsize);
	buf[rsize] = '\0';
	return rsize + 1;
}

void x0_response_write(x0_request_t* r, const char* buf, size_t size)
{
	Buffer b;
	b.push_back(buf, size);
	r->request->write<BufferSource>(b);
}
