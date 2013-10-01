#ifndef x0_capi_h
#define x0_capi_h

/*
 * This file is meant to create a C-API wrapper ontop of the x0 C++ API
 * for embedding x0 as a library into your own application.
 *
 * GOALS:
 * - support single-threaded use as well as multi-threaded use.
 * - ...
 */

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

typedef void* x0_listener_t;
typedef void* x0_request_t;

typedef int (*x0_handler_t)(x0_request_t*, void*);
// }}}
// {{{ listener setup
/**
 * Creates a new listener on given port/bind.
 *
 * @param port
 * @param bind
 * @param loop libev's loop handle to use for x0's main thread.
 */
x0_listener_t* x0_listener_create(int port, const char* bind, struct ev_loop* loop);

/**
 *
 */
int x0_listener_destroy(x0_listener_t* listener);

/**
 * Initializes that many workers to serve the requests on given listener.
 *
 * @param listener Listener handle.
 * @param count worker count to initialize (including main-worker).
 *
 * @return 0 if okay, -1 otherwise.
 */
int x0_worker_setup(x0_listener_t* listener, int count);

/**
 * @param listener Listener handle.
 * @param handler Callback handler to be invoked on every fully parsed request.
 * @param userdata Userdata to be passed to every callback in addition to the request.
 */
void x0_setup_handler(x0_listener_t* listener, x0_handler_t* handler, void* userdata);
// }}}
// {{{ request inspection
int x0_request_method(x0_request_t* r);
int x0_request_path(x0_request_t* r, char* buf, size_t size);
int x0_request_version(x0_request_t* r);

int x0_request_header_get(x0_request_t* r, const char* header_name, char* buf, size_t size);
int x0_request_header_exists(x0_request_t* r, const char* name);

/**
 * Retrieves the total number request headers.
 */
int x0_request_header_count(x0_request_t* r);

/**
 * Retrieves a request header at given offset.
 *
 * @param index the offset the request header to query at.
 * @param buf result buffer to store the request header's value into, including trailing null-byte.
 * @param size total size of the buffer in bytes that can be used to store the value, including trailing null-byte.
 * @return actual size of the request header (excluding trailing null-byte)
 */
int x0_request_header_geti(x0_request_t* r, off_t index, char* buf, size_t size);
// }}}
// {{{ response creation
/**
 * Sets a new response header, possibly overwriting an existing one, if the header name equals.
 */
int x0_response_header_set(x0_request_t* r, const char* header, const char* value);

/**
 * Sets the response status code.
 */
int x0_response_status_set(x0_request_t* r, int code);

/**
 * Sets the response status code including a custom status text.
 */
int x0_response_status_setn(x0_request_t* r, int code, const char* text);

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
int x0_response_write(x0_request_t* r, const char* buf, size_t size);
int x0_response_printf(x0_request_t* r, const char* fmt, ...);

/**
 * Marks this request as fully handled.
 *
 * This call marks the request as fully handled, causing it to flush out
 * the response in case it hasn't been sent out yet and allows possibly
 * pending (pipelined) requests to be processed, too.
 *
 * @note This implies, that the request handle becomes invalid right after this call.
 */
int x0_response_finish(x0_request_t* r);

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
 * @param body Response body buffer.
 * @param size Response body size in bytes.
 *
 * @return 0 on success, an error code otherwise.
 */
int x0_response_complete(x0_request_t* r, int code, const char* headers, size_t header_count, const char* body, size_t size);
// }}}

#endif
