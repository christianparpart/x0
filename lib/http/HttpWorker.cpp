#include <x0/http/HttpWorker.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpConnection.h>
#include <x0/StackTrace.h>

#include <ev++.h>
#include <signal.h>
#include <pthread.h>

// XXX one a connection has been passed to a worker, it is *bound* to it.

namespace x0 {

HttpWorker::HttpWorker(HttpServer& server, struct ev_loop *loop) :
	server_(server),
	loop_(loop),
	connectionLoad_(0),
	thread_(),
	exit_(false),
	queue_(),
	evNewConnection_(loop_),
	evSuspend_(loop_),
	evResume_(loop_),
	evExit_(loop_),
	fileinfo(loop_, &server_.fileinfoConfig_)
{
	evNewConnection_.set<HttpWorker, &HttpWorker::onNewConnection>(this);
	evNewConnection_.start();

	evSuspend_.set<HttpWorker, &HttpWorker::onSuspend>(this);
	evSuspend_.start();

	evResume_.set<HttpWorker, &HttpWorker::onResume>(this);
	evResume_.start();

	evExit_.set<HttpWorker, &HttpWorker::onExit>(this);
	evExit_.start();
}

HttpWorker::~HttpWorker()
{
}

void HttpWorker::run()
{
	printf("HttpWorker.run() enter\n");

	while (!exit_)
	{
		printf("%f: HttpWorker.run:\n", ev_now(loop_));
		ev_loop(loop_, EVLOOP_ONESHOT);
	}

	printf("HttpWorker.run() leave (%d)\n", exit_);
}

void HttpWorker::enqueue(std::pair<int, HttpListener *>&& client)
{
	printf("HttpWorker.enqueue() fd:%d\n", client.first);
	queue_.push_back(client);
	evNewConnection_.send();
	printf("HttpWorker.enqueue() leave\n");
}

void HttpWorker::onNewConnection(ev::async& w, int revents)
{
	printf("%f: HttpWorker.onNewConnection() enter\n", ev_now(loop_));
	printf("%s\n", StackTrace().c_str());
	std::pair<int, HttpListener *> client(queue_.front());
	queue_.pop_front();

	printf("%f: HttpWorker.onNewConnection() fd:%d\n", ev_now(loop_), client.first);

	HttpConnection *conn = new HttpConnection(*client.second, *this, client.first);

	if (conn->isClosed())
		delete conn;
	else
		conn->start();
}

void HttpWorker::onSuspend(ev::async& w, int revents)
{
	printf("%f: HttpWorker.onSuspend!\n", ev_now(loop_));
	printf("%s\n", StackTrace().c_str());
}

void HttpWorker::onResume(ev::async& w, int revents)
{
	printf("%f: HttpWorker.onResume!\n", ev_now(loop_));
	printf("%s\n", StackTrace().c_str());
}

void HttpWorker::onExit(ev::async& w, int revents)
{
	printf("%f: HttpWorker.onExit! (pending:%d)\n", ev_now(loop_), evExit_.async_pending());
	if (!evExit_.async_pending())
		return;
	printf("%s\n", StackTrace().c_str());

	if (&w == &evExit_)
		exit_ = true;
	else
		printf("oops. wrong arg!\n");
}

} // namespace x0
