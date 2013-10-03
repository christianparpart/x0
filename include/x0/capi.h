/* <src/capi.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_capi_h
#define x0_capi_h

#include <x0/Api.h>
#include <sys/param.h> /* off_t */
#include <stdint.h>
#include <ev.h>

/*
 * This file is meant to create a C-API wrapper ontop of the x0 C++ API
 * for embedding x0 as a library into your own application.
 *
 * GOALS:
 * - support single-threaded use as well as multi-threaded use.
 * - ...
 */

#if defined(__cplusplus)
extern "C" {
#endif

// {{{ types and defs
#define X0_HTTP_VERSION_UNKNOWN 0
#define X0_HTTP_VERSION_0_9 1
#define X0_HTTP_VERSION_1_0 2
#define X0_HTTP_VERSION_1_1 3
#define X0_HTTP_VERSION_2_0 4

#define X0_REQUEST_METHOD_UNKNOWN 0
#define X0_REQUEST_METHOD_GET     1
#define X0_REQUEST_METHOD_POST    2
#define X0_REQUEST_METHOD_PUT     3
#define X0_REQUEST_METHOD_DELETE  4

typedef struct x0_server_s x0_server_t;
typedef struct x0_request_s x0_request_t;

typedef void (*x0_handler_t)(x0_request_t*, void*);
// }}}
// {{{ server setup
/**
 * Creates a new server.
 *
 * @param loop libev's loop handle to use for x0's main thread.
 */
X0_API x0_server_t* x0_server_create(struct ev_loop* loop);

/**
 * Destroys an HTTP server object and all its dependant handles.
 *
 * @param server
 * @param kill 1 means it'll hard-kill all currently running connections; with 0 it'll wait until all requests have been fully served.
 */
X0_API void x0_server_destroy(x0_server_t* server, int kill);

/**
 * Creates a listener to accept incoming HTTP connections on this server instance.
 *
 * @param port TODO
 * @param bind TODO
 * @param backlog TODO
 */
X0_API int x0_listener_add(x0_server_t* server, const char* bind, int port, int backlog);

/**
 * Initializes that many workers to serve the requests on given server.
 *
 * @param server Server handle.
 * @param count worker count to initialize (including main-worker).
 *
 * @return 0 if okay, -1 otherwise.
 */
X0_API int x0_worker_setup(x0_server_t* server, int count);

/**
 * @param server Server handle.
 * @param handler Callback handler to be invoked on every fully parsed request.
 * @param userdata Userdata to be passed to every callback in addition to the request.
 */
X0_API void x0_setup_handler(x0_server_t* server, x0_handler_t handler, void* userdata);

/**
 * Configures maximum number of concurrent connections.
 */
X0_API void x0_setup_connection_limit(x0_server_t* server, size_t limit);

/**
 * Configures I/O timeouts.
 *
 * @param read read timeout in seconds.
 * @param write write timeout in seconds.
 */
X0_API void x0_setup_timeouts(x0_server_t* server, int read, int write);

/**
 * Configures HTTP keepalive.
 *
 * @param server Server handle.
 * @param count maximum number of requests that may be served via one connection.
 * @param timeout timeout in seconds to wait in maximum for the next request before terminating the connection.
 */
X0_API void x0_setup_keepalive(x0_server_t* server, int count, int timeout);

X0_API void x0_server_stop(x0_server_t* server);
// }}}
// {{{ request inspection
/**
 * Retrieves the request method as a unique ID.
 *
 * @retval X0_REQUEST_METHOD_GET GET
 * @retval X0_REQUEST_METHOD_POST POST
 * @retval X0_REQUEST_METHOD_PUT PUT
 * @retval X0_REQUEST_METHOD_DELETE DELETE
 * @retval X0_REQUEST_METHOD_UNKNOWN any other request method
 */
X0_API int x0_request_method(x0_request_t* r);

/**
 * Retrieves the request path.
 *
 * @param buf Target buffer to store the request path in.
 * @param size Capacity of the given buffer.
 *
 * @return the number of bytes stored in the target buffer, excluding trailing null-byte.
 */
X0_API size_t x0_request_path(x0_request_t* r, char* buf, size_t size);

/**
 * Retrieves the client specified HTTP version.
 *
 * @retval X0_HTTP_VERSION_UNKNOWN Unknown/unsupported HTTP version
 * @retval X0_HTTP_VERSION_0_9 HTTP version 0.9
 * @retval X0_HTTP_VERSION_1_0 HTTP version 1.0
 * @retval X0_HTTP_VERSION_1_1 HTTP version 1.1
 * @retval X0_HTTP_VERSION_2_0 not yet supported
 */
X0_API int x0_request_version(x0_request_t* r);

/**
 * Tests for existense of a given request header.
 *
 * @retval 0 Header name found.
 * @retval 1 Header name not found.
 */
X0_API int x0_request_header_exists(x0_request_t* r, const char* name);

/**
 */
X0_API int x0_request_header_get(x0_request_t* r, const char* header_name, char* buf, size_t size);

X0_API int x0_request_cookie_get(x0_request_t* r, const char* cookie, char* buf, size_t size);

/**
 * Retrieves the total number request headers.
 */
X0_API int x0_request_header_count(x0_request_t* r);

/**
 * Retrieves a request header at given offset.
 *
 * @param index the offset the request header to query at.
 * @param buf result buffer to store the request header's value into, including trailing null-byte.
 * @param size total size of the buffer in bytes that can be used to store the value, including trailing null-byte.
 * @return actual size of the request header (excluding trailing null-byte)
 */
X0_API int x0_request_header_geti(x0_request_t* r, off_t index, char* buf, size_t size);
// }}}
// {{{ response creation
/**
 * Sets a new response header, possibly overwriting an existing one, if the header name equals.
 */
X0_API void x0_response_header_set(x0_request_t* r, const char* header, const char* value);

/**
 * Sets the response status code.
 */
X0_API void x0_response_status_set(x0_request_t* r, int code);

/**
 * Writes one chunk of the response body.
 *
 * @param r Request handle to write some response to.
 * @param buf Buffer chunk to append.
 * @param size Number of bytes in \p buf.
 *
 * When writing the body, the response headers <b>MUST</b> be fully set up already,
 * as the first write will automatically flush the response status line and response headers.
 */
X0_API void x0_response_write(x0_request_t* r, const char* buf, size_t size);

X0_API int x0_response_printf(x0_request_t* r, const char* fmt, ...);

/**
 * Marks this request as fully handled.
 *
 * This call marks the request as fully handled, causing it to flush out
 * the response in case it hasn't been sent out yet and allows possibly
 * pending (pipelined) requests to be processed, too.
 *
 * @note This implies, that the request handle becomes invalid right after this call.
 */
X0_API void x0_response_finish(x0_request_t* r);

/**
 * Creates a full response to the given request and actually finishes it.
 *
 * This call is all-in-one response creation function that sets the
 * response status code, writes the response body and flushes the response
 * full out, marking it as finished.
 *
 * The request handle must not be accessed after this call as it is potentially invalid.
 *
 * @param r Request handle to create the response for.
 * @param code Response status code.
 * @param headers array of header name/value pairs.
 * @param header_count number of header name/value pairs stored.
 * @param body Response body buffer.
 * @param size Response body size in bytes.
 *
 * @return 0 on success, an error code otherwise.
 */
X0_API int x0_response_complete(x0_request_t* r, int code,
	const char** headers, size_t header_count,
	const char* body, size_t size);

/**
 * Sends given statis file.
 *
 * This cll is an all-in-one response creation function that
 * sends a local file to the client, respecting potential request headers
 * that might modify the response (for example ranged requests).
 *
 * The request handle must not be accessed after this call as it is potentially invalid.
 *
 * @param r Handle to request.
 * @param path path to local file to send to the client.
 */
X0_API int x0_response_sendfile(x0_request_t* r, const char* path);
// }}}

#if defined(__cplusplus)
}
#endif

#endif
