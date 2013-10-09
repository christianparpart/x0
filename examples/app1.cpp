/*! \brief Very simple HTTP server. Everything's done for you.
 *
 * It just serves static pages.
 */
#include <iostream>
#include <fstream>
#include <x0/http/HttpServer.h>
#include <x0/flow/FlowRunner.h>
#include <ev++.h>

class MyServer
{
private:
	struct ev_loop* loop_;
	ev::sig sigterm_;
	x0::HttpServer* http_;

public:
	MyServer() :
		loop_(ev_default_loop()),
		sigterm_(loop_),
		http_(nullptr)
	{
	}

	~MyServer()
	{
		if (sigterm_.is_active()) {
			ev_ref(loop_);
			sigterm_.stop();
		}

		delete http_;
	}

	int run()
	{
		std::clog << "Initializing ..." << std::endl;
		sigterm_.set<MyServer, &MyServer::terminate_handler>(this);
		sigterm_.start(SIGTERM);
		ev_unref(loop_);

		http_ = new x0::HttpServer(loop_);

		x0::FlowRunner::initialize();
		http_->setup("app1.conf");

		std::clog << "Running ..." << std::endl;
		int rv = http_->run();

		std::clog << "Quitting ..." << std::endl;
		return rv;
	}

private:
	void terminate_handler(ev::sig& sig, int)
	{
		std::clog << "Terminate signal received ..." << std::endl;

		ev_ref(loop_);
		sig.stop();

		http_->stop();
	}
};

int main(int argc, const char *argv[])
{
	MyServer s;
	return s.run();
}
