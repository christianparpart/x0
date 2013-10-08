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
#include <cstdarg>

using namespace x0;

struct x0_server_s
{
	HttpServer server;
	bool autoflush;

	x0_server_s(struct ev_loop* loop) :
		server(loop),
		autoflush(true)
	{}
};

struct x0_request_s
{
	x0_server_t* server;
	HttpRequest* request;

	x0_request_body_fn body_cb;
	void* body_userdata;

	x0_request_abort_fn abort_cb;
	void* abort_userdata;

	x0_request_s(HttpRequest* r, x0_server_t* s) :
		server(s),
		request(r),
		body_cb(nullptr),
		body_userdata(nullptr),
		abort_cb(nullptr),
		abort_userdata(nullptr)
	{
		request->connection.setAutoFlush(server->autoflush);
	}

	void bodyCallback(const BufferRef& ref) {
		if (body_cb) {
			body_cb(this, ref.data(), ref.size(), body_userdata);
		}
	}

	static void abortCallback(void* p) {
		x0_request_s* self = (x0_request_s*) p;
		if (self->abort_cb) {
			self->abort_cb(self->abort_userdata);
		}
	}
};

x0_server_t* x0_server_create(struct ev_loop* loop)
{
	auto s = new x0_server_t(loop);

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

int x0_listener_add(x0_server_t* server, const char* bind, int port, int backlog)
{
	auto listener = server->server.setupListener(bind, port, 128);
	if (listener == nullptr)
		return -1;

	return 0;
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

void x0_setup_handler(x0_server_t* server, x0_request_handler_fn handler, void* userdata)
{
	server->server.requestHandler = [=](HttpRequest* r) -> bool {
		auto rr = new x0_request_t(r, server);
		handler(rr, userdata);
		return true;
	};
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

void x0_setup_autoflush(x0_server_t* server, int value)
{
	server->autoflush = value;
}

// --------------------------------------------------------------------------
// REQUEST SETUP

void x0_request_body_callback(x0_request_t* r, x0_request_body_fn handler, void* userdata)
{
	r->request->setBodyCallback<x0_request_t, &x0_request_t::bodyCallback>(r);
	r->body_cb = handler;
	r->body_userdata = userdata;
}

void x0_request_abort_callback(x0_request_t* r, x0_request_abort_fn handler, void* userdata)
{
	r->request->setAbortHandler(&x0_request_t::abortCallback, r);
	r->abort_cb = handler;
	r->abort_userdata = userdata;
}

void x0_request_post(x0_request_t* r, x0_request_post_fn fn, void* userdata)
{
	r->request->post([=]() {
		fn(r, userdata);
	});
}

// --------------------------------------------------------------------------
// REQUEST

int x0_request_method_id(x0_request_t* r)
{
	if (r->request->method == "GET")
		return X0_REQUEST_METHOD_GET;

	if (r->request->method == "POST")
		return X0_REQUEST_METHOD_POST;

	if (r->request->method == "PUT")
		return X0_REQUEST_METHOD_PUT;

	if (r->request->method == "DELETE")
		return X0_REQUEST_METHOD_DELETE;

	return X0_REQUEST_METHOD_UNKNOWN;
}

size_t x0_request_method(x0_request_t* r, char* buf, size_t size)
{
	if (!size)
		return 0;

	size_t n = std::min(r->request->method.size(), size - 1);
	memcpy(buf, r->request->method.data(), n);
	buf[n] = '\0';
	return n;
}

size_t x0_request_path(x0_request_t* r, char* buf, size_t size)
{
	size_t rsize = std::min(size - 1, r->request->path.size());
	memcpy(buf, r->request->path.data(), rsize);
	buf[rsize] = '\0';
	return rsize + 1;
}

int x0_request_version(x0_request_t* r)
{
	static const struct {
		int major;
		int minor;
		int code;
	} versions[] = {
		{ 0, 9, X0_HTTP_VERSION_0_9 },
		{ 1, 0, X0_HTTP_VERSION_1_0 },
		{ 1, 1, X0_HTTP_VERSION_1_1 },
		{ 2, 0, X0_HTTP_VERSION_2_0 },
	};

	int major = r->request->httpVersionMajor;
	int minor = r->request->httpVersionMinor;

	for (const auto& version: versions)
		if (version.major == major && version.minor == minor)
			return version.code;

	return X0_HTTP_VERSION_UNKNOWN;
}

int x0_request_header_exists(x0_request_t* r, const char* name)
{
	for (const auto& header: r->request->requestHeaders)
		if (header.name == name)
			return 1;

	return 0;
}

int x0_request_header_get(x0_request_t* r, const char* name, char* buf, size_t size)
{
	if (size) {
		for (const auto& header: r->request->requestHeaders) {
			if (header.name == name) {
				size_t len = std::min(header.value.size(), size - 1);
				memcpy(buf, header.value.data(), len);
				buf[len] = '\0';
				return len;
			}
		}
	}

	return 0;
}

int x0_request_cookie_get(x0_request_t* r, const char* cookie, char* buf, size_t size)
{
	return 0; // TODO cookie retrieval
}

int x0_request_header_count(x0_request_t* r)
{
	return r->request->requestHeaders.size();
}

int x0_request_header_geti(x0_request_t* r, off_t index, char* buf, size_t size)
{
	if (size && (size_t)index < r->request->requestHeaders.size()) {
		const auto& header = r->request->requestHeaders[index];
		size_t len = std::min(header.value.size(), size - 1);
		memcpy(buf, header.value.data(), len);
		buf[len] = '\0';
		return len;
	}
	return 0;
}

// --------------------------------------------------------------------------
// RESPONSE

void x0_response_flush(x0_request_t* r)
{
	r->request->connection.flush();
}

void x0_response_status_set(x0_request_t* r, int code)
{
	r->request->status = static_cast<HttpStatus>(code);
}

void x0_response_header_set(x0_request_t* r, const char* name, const char* value)
{
	r->request->responseHeaders.overwrite(name, value);
}

void x0_response_write(x0_request_t* r, const char* buf, size_t size)
{
	Buffer b;
	b.push_back(buf, size);
	r->request->write<BufferSource>(b);
}

void x0_response_printf(x0_request_t* r, const char* fmt, ...)
{
	Buffer b;
	va_list ap;

	va_start(ap, fmt);
	b.vprintf(fmt, ap);
	va_end(ap);

	r->request->write<BufferSource>(b);
}

void x0_response_vprintf(x0_request_t* r, const char* fmt, va_list args)
{
	Buffer b;
	va_list va;

	va_copy(va, args);
	b.vprintf(fmt, va);
	va_end(va);

	r->request->write<BufferSource>(b);
}

void x0_response_finish(x0_request_t* r)
{
	r->request->finish();
	delete r;
}

void x0_response_sendfile(x0_request_t* r, const char* path)
{
	r->request->sendfile(path);
}
