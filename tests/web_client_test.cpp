#include <x0/web_client.hpp>
#include <x0/buffer.hpp>

#include <functional> // bind(), placeholders
#include <iostream>   // clog
#include <cctype>     // atoi()

#include <ev++.h>

void on_response(const x0::buffer_ref& protocol, const x0::buffer_ref& code, const x0::buffer_ref& text)
{
	std::clog << "S< " << protocol.str() << " " << code.str() << ' ' << text.str() << std::endl;
}

void on_header(const x0::buffer_ref& name, const x0::buffer_ref& value)
{
	std::clog << "S< " << name.str() << ": " << value.str() << std::endl;
}

void on_content(const x0::buffer_ref& chunk)
{
	std::clog << "S< content of length: " << chunk.size() << std::endl
			  << chunk.str() << std::endl
			  << "S< content end." << std::endl;
}

void on_complete()
{
	std::clog << "S< complete." << std::endl;
	ev_unloop(ev_default_loop(0), EVUNLOOP_ALL);
}

int main(int argc, const char *argv[])
{
	const int request_count = 2;
	struct ev_loop *loop = ev_default_loop(0);

	x0::web_client client(loop);

	using namespace std::placeholders;
	client.on_response = std::bind(&on_response, _1, _2, _3);
	client.on_header = std::bind(&on_header, _1, _2);
	client.on_content = std::bind(&on_content, _1);
	client.on_complete = std::bind(&on_complete);
	client.keepalive_timeout = 5;

	if (argc == 4)
		client.open(argv[1], std::atoi(argv[2]));
	else
		client.open("localhost", 80);

	if (client.state() == x0::web_client::DISCONNECTED)
	{
		std::clog << "Could not connect to server: " << client.message() << std::endl;
		return -1;
	}

	for (int i = 0; i < request_count; ++i)
	{
		if (argc == 4)
			client.pass_request("GET", argv[3]);
		else
			client.pass_request("GET", "/");

		client.pass_header("Host", "localhost"); // required field for HTTP/1.1
		client.pass_header("User-Agent", "x0");
		client.pass_header("X-Foo", "bar");

		client.commit(i == (request_count - 1)); // pass true on last iteration, false otherwise.
	}

	ev_loop(loop, 0);

	if (!client.message().empty())
	{
		std::clog << "connection error: " << client.message() << std::endl;
	}

	return 0;
}
