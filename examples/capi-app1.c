#include <x0/capi.h>
#include <getopt.h>
#include <ev.h>
#include <stdio.h>

void handler(x0_request_t* r, void* userdata)
{
	x0_server_t* server = (x0_server_t*) userdata;
	char body[] = "This Is Sparta!\n";

	char path[1024];
	x0_request_path(r, path, sizeof(path));

	printf("Request-Path: %s\n", path);

	x0_response_status_set(r, 200);
	x0_response_header_set(r, "Content-Type", "text/plain");
	x0_response_write(r, body, sizeof(body) - 1);
	x0_response_finish(r);

	if (strcmp(path, "/halt") == 0) {
		x0_server_stop(server);
	}
}

int main(int argc, const char* argv[])
{
	x0_server_t* server;
	struct ev_loop* loop;

	loop = ev_default_loop(0);
	server = x0_server_create(8080, "0.0.0.0", loop);

	x0_setup_timeouts(server, /*read*/ 30, /*write*/ 10);
	x0_setup_keepalive(server, /*count*/ 5, /*timeout*/ 8);

	x0_setup_handler(server, &handler, server);

	ev_run(loop, 0);

	x0_server_destroy(server, 0);

	return 0;
}
