#include <x0/WebClient.h>
#include <x0/Buffer.h>
#include <x0/http/HttpStatus.h>
#include <x0/Url.h>

#include <functional> // bind(), placeholders
#include <iostream>   // clog
#include <cctype>     // atoi()

#include <ev++.h>

int request_count = 2;

void on_response(int vmajor, int vminor, int code, x0::BufferRef&& text)
{
	std::clog << "S< HTTP/" << vmajor << "." << vminor << " " << code << ' ' << text.str() << std::endl;
}

void on_header(x0::BufferRef&& name, x0::BufferRef&& value)
{
	std::clog << "S< " << name.str() << ": " << value.str() << std::endl;
}

bool on_content(x0::BufferRef&& chunk)
{
#if 0
	std::clog << "S< content of length: " << chunk.size() << std::endl
			  << chunk.str() << std::endl
			  << "S< content end." << std::endl;
#else
	std::clog << chunk.str();
#endif
	return true;
}

bool on_complete(x0::WebClient *client)
{
	std::clog << "S< complete." << std::endl;

	static int i = 0;

	++i;

	if (i == request_count)
	{
		std::clog << "S< this was the last response." << std::endl;
		//client->close();
		ev_unloop(ev_default_loop(0), EVUNLOOP_ALL);
		return false;
	}
	return true;
}

int main(int argc, const char *argv[])
{
	struct ev_loop *loop = ev_default_loop(0);

	x0::WebClient client(loop);

	using namespace std::placeholders;
	client.on_response = std::bind(&on_response, _1, _2, _3, _4);
	client.on_header = std::bind(&on_header, _1, _2);
	client.on_content = std::bind(&on_content, _1);
	client.on_complete = std::bind(&on_complete, &client);
	client.keepaliveTimeout = 5;

	std::string url("http://xzero.io/cgi-bin/cgi-test.cgi");
	if (argc > 1)
		url = argv[1];

	std::string protocol;
	std::string hostname;
	int port = 80;
	std::string path;

	if (!x0::parseUrl(url, protocol, hostname, port, path))
	{
		std::cerr << "URL syntax error" << std::endl;
		return 1;
	}

	client.open(hostname, port);

	if (client.state() == x0::WebClient::DISCONNECTED)
	{
		std::clog << "Could not connect to server: " << client.lastError().message() << std::endl;
		return -1;
	}

	for (int i = 0; i < request_count; ++i)
	{
		client.writeRequest("GET", path);

		client.writeHeader("Host", hostname);
		client.writeHeader("User-Agent", "x0");
		client.writeHeader("X-Foo", "bar");

		client.commit(i == (request_count - 1)); // pass true on last iteration, false otherwise.
	}

	ev_loop(loop, 0);

	if (client.lastError())
	{
		std::clog << "connection error: " << client.lastError() << std::endl;
	}

	return 0;
}
