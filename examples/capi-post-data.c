#include <x0/capi.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <ev.h>

/*
 * SAMPLE HTTP REQUESTS:
 *
 *   curl http://localhost:8080/
 *   curl -X POST http://localhost:8080/upload --data-binary @/etc/shadow
 *   curl http://localhost:8080/quit
 */

typedef struct {
	char* body_data;
	size_t body_size;
	x0_server_t* server;
} request_udata_t;

void process_request(x0_request_t* r, request_udata_t* udata)
{
	char method[16];
	char path[1024];

	x0_request_method(r, method, sizeof(method));
	x0_request_path(r, path, sizeof(path));

	printf("%s %s\n", method, path);
	if (udata->body_size) {
		fwrite(udata->body_data, udata->body_size, 1, stdout);
		fflush(stdout);
	}

	x0_response_status_set(r, 200);
	x0_response_header_set(r, "Content-Type", "text/plain");

	static const char msg[] = "This Is Sparta!\n";
	x0_response_write(r, msg, sizeof(msg) - 1);
	x0_response_finish(r);

	if (strcmp(path, "/quit") == 0) {
		x0_server_stop(udata->server);
	}

	free(udata->body_data);
	free(udata);
}

void body_handler(x0_request_t* r, const char* data, size_t size, void* userdata)
{
	request_udata_t* udata = (request_udata_t*) userdata;

	if (size) {
		udata->body_data = realloc(udata->body_data, udata->body_size + size);
		memcpy(udata->body_data + udata->body_size, data, size);
		udata->body_size += size;
	} else {
		process_request(r, udata);
	}
}

void request_handler(x0_request_t* r, void* userdata)
{
	x0_server_t* server = (x0_server_t*) userdata;

	request_udata_t* udata = malloc(sizeof(request_udata_t));
	udata->body_data = NULL;
	udata->body_size = 0;
	udata->server = server;

	x0_request_body_callback(r, &body_handler, udata);
}

int main(int argc, const char* argv[])
{
	const char* bind = "0.0.0.0";
	int port = 8080;
	x0_server_t* server;
	struct ev_loop* loop;

	loop = ev_default_loop(0);
	server = x0_server_create(loop);

	if (x0_listener_add(server, bind, port, 128) < 0) {
		perror("x0_listener_add");
		x0_server_destroy(server, 0);
		return 1;
	}

	x0_setup_timeouts(server, /*read*/ 30, /*write*/ 10);
	x0_setup_keepalive(server, /*count*/ 5, /*timeout*/ 8);
	x0_setup_handler(server, /*callback*/ &request_handler, /*userdata*/ server);

	printf("[HTTP] Listening on %s port %d\n", bind, port);

	ev_run(loop, 0);

	printf("[HTTP] Shutting down\n");

	x0_server_destroy(server, 0);

	return 0;
}
