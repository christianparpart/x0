#include <x0/capi.h>
#include <getopt.h>
#include <ev.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h> // usleep()

void finish_request(x0_request_t* r, void* userdata) {
	x0_response_finish(r);
}

void* async_handler(void* p) {
	x0_request_t* r = (x0_request_t *) p;
	static const char body[] = "This Is Sparta!\n";

	char slen[32];
	snprintf(slen, sizeof(slen), "%zu", strlen(body));
	x0_response_header_set(r, "Content-Length", slen);

//	usleep(1000);
	x0_response_write(r, body, sizeof(body) - 1);
	x0_request_post(r, &finish_request, NULL);

	return NULL;
}

void handler(x0_request_t* r, void* userdata) {
	x0_server_t* server = (x0_server_t*) userdata;
	char path[1024];

	x0_request_path(r, path, sizeof(path));

	x0_response_status_set(r, 200);
	x0_response_header_set(r, "Content-Type", "text/plain");

	if (strncmp(path, "/sendfile", 9) == 0) {
		x0_response_sendfile(r, path + 9);
		x0_response_finish(r);
	} else if (strcmp(path, "/quit") == 0) {
		x0_response_header_set(r, "Content-Length", "4");
		x0_response_printf(r, "Bye\n");
		x0_response_finish(r);
		x0_server_stop(server);
	} else {
		pthread_t pth;
		pthread_create(&pth, NULL, &async_handler, r);
		pthread_setname_np(pth, "worker");
		pthread_detach(pth);
	}
}

int main(int argc, const char* argv[]) {
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

	x0_setup_autoflush(server, 0 /*false*/);
	x0_setup_handler(server, /*callback*/ &handler, /*userdata*/ server);

	printf("[HTTP] Listening on %s port %d\n", bind, port);

	x0_server_run(server);
	x0_server_destroy(server, 0);

	return 0;
}
