#ifndef sw_x0_c_h
#define sw_x0_c_h (1)

#include <sys/types.h>

/* x0-c API */

// flow
typedef enum
{
	X0_FLOW_TYPE_UNSPECIFIED = 0,
	X0_FLOW_TYPE_NUMBER = 1,
	X0_FLOW_TYPE_STRING = 2,
	X0_FLOW_TYPE_BUFFER = 3,
	X0_FLOW_TYPE_BOOLEAN = 4,
	X0_FLOW_TYPE_ARRAY = 5,
	X0_FLOW_TYPE_HASH = 6
} x0_flow_type;

typedef struct __attribute__((packed)) _flow_value
{
	x0_flow_type type;

	const char *i8;
	uint64_t ui64;
	struct _flow_value *array;

	// ... TODO ...
} flow_value_t;

/* x0 types */
typedef struct _x0_buf
{
	const char *buf;
	size_t len;
} x0_buf_t;

typedef struct _x0_request_t x0_request_t;
typedef struct _x0_server_t x0_server_t;

typedef void (*x0_request_body_cb_t)(x0_request_t *, const char *chunk, size_t len, void *cx);
typedef void (*x0_request_cb_t)(x0_request_t *, void *cx);

typedef void (*x0_setup_function_cb_t)(flow_value_t *result, int argc, flow_value_t *args, void *cx);
typedef void (*x0_request_function_cb_t)(flow_value_t *result, int argc, flow_value_t *args, x0_request_t *r, void *cx);
typedef int (*x0_request_handler_cb_t)(int argc, const flow_value_t *args, x0_request_t *r, void *cx);

/* connnection-level */
const char *x0_remoteip(x0_request_t *);
int x0_remoteport(x0_request_t *);
const char *x0_localip(x0_request_t *);
int x0_localport(x0_request_t *);

/* request line */
x0_buf_t x0_request_method(x0_request_t *r);
x0_buf_t x0_request_uri(x0_request_t *r);
x0_buf_t x0_request_path(x0_request_t *r);

/* request headers */
x0_buf_t x0_request_header_get(x0_request_t *r, const char *name, int *len);
size_t x0_request_header_count(x0_request_t *r);
x0_buf_t x0_request_header_at(x0_request_t *r, off_t index);

/* request body */
int x0_request_content_available(x0_request_t *r);
void x0_request_read_async(x0_request_t *r, x0_request_body_cb_t cb, void *cx);

/* response headers */
const char *x0_response_header_get(x0_request_t *r, const char *name);
size_t x0_response_header_count(x0_request_t *r);
x0_buf_t x0_response_header_at(x0_request_t *r, off_t index);
void x0_response_header_append(x0_request_t *r, const char *name, const char *append_value);
void x0_response_header_overwrite(x0_request_t *r, const char *name, const char *value);
void x0_response_header_remove(x0_request_t *r, const char *name);

/* response body */
void x0_write(x0_request_t *r, const char *buf, size_t len);
void x0_write_fd(x0_request_t *r, int fd, off_t offset, size_t count);
void x0_printf(x0_request_t *r, const char *fmt, ...) __attribute__(format(2, 3));
void x0_puts(x0_request_t *r, const char *cstr);
void x0_flush(x0_request_t *t, x0_request_cb_t cb, void *cx);
void x0_finish(x0_request_t *r);

/* flow */
int x0_config_register_setup_function(x0_server_t *s, const char *name, x0_flow_type t, x0_setup_function_cb_t cb, void *cx);
int x0_config_register_setup_property(x0_server_t *s, const char *name, x0_flow_type t, x0_setup_function_cb_t cb, void *cx);
int x0_config_register_function(x0_server_t *s, const char *name, x0_flow_type t, x0_function_cb_t cb, void *cx);
int x0_config_register_property(x0_server_t *s, const char *name, x0_flow_type t, x0_property_cb_t cb, void *cx);
int x0_config_register_handler(x0_server_t *s, const char *name, x0_flow_type t, x0_handler_cb_t cb, void *cx);
int x0_config_unregister(x0_server_t *s, const char *name);

/* x0 plugin */
typedef struct _x0_plugin {
	const char *name;

	int (*initialize)(x0_server_t *);
	int (*post_config)(x0_server_t *);
	int (*post_check)(x0_server_t *);
} x0_plugin_t;

#define X0_C_PLUGIN(name, struct_name) \
	x0_plugin ...

#endif
