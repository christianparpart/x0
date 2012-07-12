#include <x0/http/HttpMessageProcessor.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpServer.h>
#include <x0/cache/Redis.h>
#include <x0/LocalStream.h>
#include <x0/Socket.h>
#include <stdio.h>
#include <string.h>
#include <ev++.h>

using namespace x0;

int main(int argc, const char *argv[])
{
	struct ev_loop* loop = ev_default_loop();
	Redis cli(loop);
	cli.open("localhost", 6379);

	if (argc == 1) {
		argc = 2;
		static const char *argv_[] = { argv[0], "foo", 0 };
		argv = argv_;
	}

	if (argc > 2)
		if (!cli.set(argv[1], argv[2]))
			printf("couldn't store redis key/value pair\n");

	if (argc >= 2) {
		Buffer buf;

		if (!cli.get(argv[1], buf))
			printf("couldn't retrieve redis value\n");
		else
			printf("%s: %s\n", argv[1], buf.c_str());
	}

	return 0;
}
