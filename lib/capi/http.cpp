// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/capi.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <x0/io/FixedBufferSink.h>
#include <x0/DebugLogger.h>
#include <cstdarg>

using namespace x0;

struct x0_server_s {
  HttpServer server;
  bool autoflush;

  x0_server_s(struct ev_loop* loop) : server(loop), autoflush(true) {}
};

struct x0_request_s {
  x0_server_t* server;
  HttpRequest* request;

  x0_request_abort_fn abort_cb;
  void* abort_userdata;

  x0_request_s(HttpRequest* r, x0_server_t* s)
      : server(s), request(r), abort_cb(nullptr), abort_userdata(nullptr) {
    request->connection.setAutoFlush(server->autoflush);
  }
};

x0_server_t* x0_server_create(struct ev_loop* loop) {
  DebugLogger::get().configure("XZERO_DEBUG");

  auto s = new x0_server_t(loop);

  s->server.requestHandler = [](HttpRequest* r) {
    BufferSource body(
        "Hello, I am lazy to serve anything; I wasn't configured properly\n");

    r->status = HttpStatus::InternalServerError;

    char buf[40];
    snprintf(buf, sizeof(buf), "%zu", body.size());
    r->responseHeaders.push_back("Content-Length", buf);

    r->responseHeaders.push_back("Content-Type", "plain/text");
    r->write<BufferSource>(body);

    r->finish();
  };

  return s;
}

int x0_listener_add(x0_server_t* server, const char* bind, int port,
                    int backlog) {
  auto listener = server->server.setupListener(bind, port, 128);
  if (listener == nullptr) return -1;

  return 0;
}

void x0_server_destroy(x0_server_t* server, int kill) {
  if (!kill) {
    // TODO wait until all current connections have been finished, if more than
    // one worker has been set up
  }
  delete server;
}

void x0_server_run(x0_server_t* server) {
  DebugLogger::get().configure("XZERO_DEBUG");
  server->server.run();
}

void x0_server_stop(x0_server_t* server) { server->server.stop(); }

int x0_setup_worker_count(x0_server_t* server, int count) {
  ssize_t current = server->server.workers().size();
  while (current < count) {
    server->server.createWorker();
    current = server->server.workers().size();
  }
  return 0;
}

void x0_setup_handler(x0_server_t* server, x0_request_handler_fn handler,
                      void* userdata) {
  server->server.requestHandler = [=](HttpRequest* r) {
    auto rr = new x0_request_t(r, server);
    handler(rr, userdata);
  };
}

void x0_setup_connection_limit(x0_server_t* li, size_t limit) {
  li->server.maxConnections = limit;
}

void x0_setup_timeouts(x0_server_t* server, int read, int write) {
  server->server.maxReadIdle = TimeSpan::fromSeconds(read);
  server->server.maxWriteIdle = TimeSpan::fromSeconds(write);
}

void x0_setup_keepalive(x0_server_t* server, int count, int timeout) {
  server->server.maxKeepAliveRequests = count;
  server->server.maxKeepAlive = TimeSpan::fromSeconds(timeout);
}

void x0_setup_autoflush(x0_server_t* server, int value) {
  server->autoflush = value;
}

void x0_server_tcp_cork_set(x0_server_t* server, int flag) {
  server->server.tcpCork = flag;
}

void x0_server_tcp_nodelay_set(x0_server_t* server, int flag) {
  server->server.tcpNoDelay = flag;
}

// --------------------------------------------------------------------------
// REQUEST SETUP

void x0_request_abort_callback(x0_request_t* r, x0_request_abort_fn handler,
                               void* userdata) {
  r->request->setAbortHandler([r]() {
    if (r->abort_cb) {
      r->abort_cb(r->abort_userdata);
    }
  });
}

void x0_request_post(x0_request_t* r, x0_request_post_fn fn, void* userdata) {
  r->request->post([=]() { fn(r, userdata); });
}

void x0_request_ref(x0_request_t* r) { r->request->connection.ref(); }

void x0_request_unref(x0_request_t* r) { r->request->connection.unref(); }

// --------------------------------------------------------------------------
// REQUEST

int x0_request_method_id(x0_request_t* r) {
  if (r->request->method == "GET") return X0_REQUEST_METHOD_GET;

  if (r->request->method == "POST") return X0_REQUEST_METHOD_POST;

  if (r->request->method == "PUT") return X0_REQUEST_METHOD_PUT;

  if (r->request->method == "DELETE") return X0_REQUEST_METHOD_DELETE;

  return X0_REQUEST_METHOD_UNKNOWN;
}

size_t x0_request_method(x0_request_t* r, char* buf, size_t size) {
  if (!size) return 0;

  size_t n = std::min(r->request->method.size(), size - 1);
  memcpy(buf, r->request->method.data(), n);
  buf[n] = '\0';
  return n;
}

size_t x0_request_path(x0_request_t* r, char* buf, size_t size) {
  if (!size) return 0;

  size_t rsize = std::min(size - 1, r->request->path.size());
  memcpy(buf, r->request->path.data(), rsize);
  buf[rsize] = '\0';
  return rsize + 1;
}

size_t x0_request_query(x0_request_t* r, char* buf, size_t size) {
  if (!size) return 0;

  size_t rsize = std::min(size - 1, r->request->query.size());
  memcpy(buf, r->request->query.data(), rsize);
  buf[rsize] = '\0';
  return rsize + 1;
}

int x0_request_version(x0_request_t* r) {
  static const struct {
    int major;
    int minor;
    int code;
  } versions[] = {{0, 9, X0_HTTP_VERSION_0_9},
                  {1, 0, X0_HTTP_VERSION_1_0},
                  {1, 1, X0_HTTP_VERSION_1_1},
                  {2, 0, X0_HTTP_VERSION_2_0}, };

  int major = r->request->httpVersionMajor;
  int minor = r->request->httpVersionMinor;

  for (const auto& version : versions)
    if (version.major == major && version.minor == minor) return version.code;

  return X0_HTTP_VERSION_UNKNOWN;
}

int x0_request_header_exists(x0_request_t* r, const char* name) {
  for (const auto& header : r->request->requestHeaders)
    if (header.name == name) return 1;

  return 0;
}

int x0_request_header_get(x0_request_t* r, const char* name, char* buf,
                          size_t size) {
  if (size) {
    for (const auto& header : r->request->requestHeaders) {
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

int x0_request_cookie(x0_request_t* r, const char* cookie, char* buf,
                      size_t size) {
  if (size == 0) return 0;

  std::string value = r->request->cookie(cookie);
  size_t n = std::min(value.size(), size - 1);
  memcpy(buf, value.data(), n);
  buf[n] = '\0';

  return n;
}

int x0_request_header_count(x0_request_t* r) {
  return r->request->requestHeaders.size();
}

int x0_request_header_name_geti(x0_request_t* r, off_t index, char* buf,
                                size_t size) {
  if (size && (size_t)index < r->request->requestHeaders.size()) {
    const auto& header = r->request->requestHeaders[index];
    size_t len = std::min(header.name.size(), size - 1);
    memcpy(buf, header.name.data(), len);
    buf[len] = '\0';
    return len;
  }
  return 0;
}

int x0_request_header_value_geti(x0_request_t* r, off_t index, char* buf,
                                 size_t size) {
  if (size && (size_t)index < r->request->requestHeaders.size()) {
    const auto& header = r->request->requestHeaders[index];
    size_t len = std::min(header.value.size(), size - 1);
    memcpy(buf, header.value.data(), len);
    buf[len] = '\0';
    return len;
  }
  return 0;
}

size_t x0_request_body_get(x0_request_t* r, char* buf, size_t capacity) {
  std::unique_ptr<Source>& body = r->request->body();
  FixedBuffer wrapper(buf, capacity, 0);
  FixedBufferSink sink(wrapper);

  body->sendto(sink);

  size_t size = std::min(sink.size(), capacity);
  memcpy(buf, sink.buffer().data(), size);

  return size;
}

// --------------------------------------------------------------------------
// RESPONSE

void x0_response_flush(x0_request_t* r) { r->request->connection.flush(); }

void x0_response_status_set(x0_request_t* r, int code) {
  r->request->status = static_cast<HttpStatus>(code);
}

void x0_response_header_set(x0_request_t* r, const char* name,
                            const char* value) {
  r->request->responseHeaders.overwrite(name, value);
}

void x0_response_header_append(x0_request_t* r, const char* name,
                               const char* value) {
  auto header = r->request->responseHeaders.findHeader(name);
  if (!header) {
    r->request->responseHeaders.push_back(name, value);
    return;
  }

  std::string& hval = header->value;
  if (hval.empty()) {
    hval = value;
    return;
  }

  if (std::isspace(hval[hval.size() - 1])) {
    hval += value;
    return;
  }

  hval += ' ';
  hval += value;
}

void x0_response_write(x0_request_t* r, const char* buf, size_t size) {
  Buffer b;
  b.push_back(buf, size);
  r->request->write<BufferSource>(b);
}

void x0_response_printf(x0_request_t* r, const char* fmt, ...) {
  Buffer b;
  va_list ap;

  va_start(ap, fmt);
  b.vprintf(fmt, ap);
  va_end(ap);

  r->request->write<BufferSource>(b);
}

void x0_response_vprintf(x0_request_t* r, const char* fmt, va_list args) {
  Buffer b;
  va_list va;

  va_copy(va, args);
  b.vprintf(fmt, va);
  va_end(va);

  r->request->write<BufferSource>(b);
}

void x0_response_finish(x0_request_t* r) {
  if (!r->server->autoflush) {
    r->request->connection.setAutoFlush(true);
    r->request->connection.flush();
  }

  r->request->finish();

  delete r;
}

void x0_response_sendfile(x0_request_t* r, const char* path) {
  printf("sendfile: '%s'\n", path);
  r->request->sendfile(path);
}
