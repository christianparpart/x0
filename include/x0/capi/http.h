/* <x0/capi/http.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 *
 * THIS C API IS EXPERIMENTAL AND SUBJECT TO CHANGE WHENEVER IT FEELS TO BE
 * AN IMPROVEMENT.
 */

#ifndef x0_capi_http_h
#define x0_capi_http_h

#include <x0/Api.h>
#include <sys/param.h> /* off_t */
#include <stdint.h>
#include <ev.h>

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

typedef void (*x0_request_handler_fn)(x0_request_t*, void* userdata);
typedef void (*x0_request_abort_fn)(void* userdata);
typedef void (*x0_request_post_fn)(x0_request_t* r, void* userdata);

// }}}
// {{{ server management
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
 * Runs given server's main worker until stopped.
 *
 * @note It is highly recommended to us this call instead of libev's ev_run() as this call is doing a little more management stuff around that call.
 */
X0_API void x0_server_run(x0_server_t* server);

/**
 * Signals graceful server stop.
 */
X0_API void x0_server_stop(x0_server_t* server);

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
X0_API int x0_setup_worker_count(x0_server_t* server, int count);

/**
 * @param server Server handle.
 * @param handler Callback handler to be invoked on every fully parsed request.
 * @param userdata Userdata to be passed to every callback in addition to the request.
 */
X0_API void x0_setup_handler(x0_server_t* server, x0_request_handler_fn handler, void* userdata);

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

/**
 * Sets whether or not to auto-flush given response.
 *
 * @param s HTTP server to set the flag for.
 * @param value boolean indicating whether or not to auto-flush response chunks.
 */
X0_API void x0_setup_autoflush(x0_server_t* s, int value);

/**
 * Sets whether or not to enable \c TCP_CORK on HTTP client connections when writing the response.
 *
 * @param s HTTP server to set the flag for.
 * @param value boolean indicating whether or not to set this flag.
 */
X0_API void x0_server_tcp_cork_set(x0_server_t* server, int flag);

/**
 * Sets whether or not to enable \c TCP_NODELAY on HTTP client connections when writing the response.
 *
 * @param s HTTP server to set the flag for.
 * @param value boolean indicating whether or not to set this flag.
 */
X0_API void x0_server_tcp_nodelay_set(x0_server_t* server, int flag);

// }}}
// {{{ request management
/**
 * Installs a client abort handler for given request.
 *
 * @param r request to install the client abort handler for
 * @param handler callback for handling the client aborts
 * @param userdata custom userdata pointer to be passed to the handler
 */
X0_API void x0_request_abort_callback(x0_request_t* r, x0_request_abort_fn handler, void* userdata);

/**
 * Pushes a callback into the request's corresponding worker thread for execution.
 *
 * It is ensured that given callback is invoked from within the request's worker thread.
 *
 * @param r Request to enqueue the post handler to.
 * @param fn Callback to be invoked from within the request's worker thread.
 * @param userdata Userdata to be passed additionally to the callback.
 */
X0_API void x0_request_post(x0_request_t* r, x0_request_post_fn fn, void* userdata);

X0_API void x0_request_ref(x0_request_t* r);
X0_API void x0_request_unref(x0_request_t* r);
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
X0_API int x0_request_method_id(x0_request_t* r);

/**
 * Retrieves the request method.
 *
 * @param buf buffer to write the method string into (including trailing zero-byte)
 * @param size buffer capacity.
 *
 * @return number of bytes written to the output buffer.
 */
X0_API size_t x0_request_method(x0_request_t* r, char* buf, size_t size);

/**
 * Retrieves the request path.
 *
 * @param buf Target buffer to store the request path in.
 * @param size Capacity of the given buffer.
 *
 * @return the number of bytes stored in the target buffer, excluding trailing zero-byte.
 */
X0_API size_t x0_request_path(x0_request_t* r, char* buf, size_t size);

/**
 * Retrieves the unparsed query part.
 *
 * @param buf Target buffer to store the request query in.
 * @param size Capacity of the given buffer.
 *
 * @return the number of bytes stored in the target buffer, excluding trailing zero-byte.
 */
X0_API size_t x0_request_query(x0_request_t* r, char* buf, size_t size);

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

X0_API int x0_request_cookie(x0_request_t* r, const char* cookie, char* buf, size_t size);

/**
 * Retrieves the total number request headers.
 */
X0_API int x0_request_header_count(x0_request_t* r);

/**
 * Retrieves a request header at given offset.
 *
 * @param index the offset the request header to query at.
 * @param buf result buffer to store the request header's value into, including trailing zero-byte.
 * @param size total size of the buffer in bytes that can be used to store the value, including trailing zero-byte.
 * @return actual size of the request header (excluding trailing zero-byte)
 */
X0_API int x0_request_header_value_geti(x0_request_t* r, off_t index, char* buf, size_t size);

X0_API int x0_request_header_name_geti(x0_request_t* r, off_t index, char* buf, size_t size);
// }}}
// {{{ response creation

/**
 * Immediately flushes any pending response data chunks.
 */
X0_API void x0_response_flush(x0_request_t* r);

/**
 * Sets the response status code.
 */
X0_API void x0_response_status_set(x0_request_t* r, int code);

/**
 * Sets a new response header, possibly overwriting an existing one, if the header name equals.
 */
X0_API void x0_response_header_set(x0_request_t* r, const char* header, const char* value);

/**
 * Appends a value to an existing header or creates a new header if it doesn't exist yet.
 */
X0_API void x0_response_header_append(x0_request_t* r, const char* header, const char* value);

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

X0_API void x0_response_printf(x0_request_t* r, const char* fmt, ...);

X0_API void x0_response_vprintf(x0_request_t* r, const char* fmt, va_list args);

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
 * Sends given statis file.
 *
 * This cll is an all-in-one response creation function that
 * sends a local file to the client, respecting potential request headers
 * that might modify the response (for example ranged requests).
 *
 * @param r Handle to request.
 * @param path path to local file to send to the client.
 */
X0_API void x0_response_sendfile(x0_request_t* r, const char* path);
// }}}

#if defined(__cplusplus)
}
#endif

#endif
