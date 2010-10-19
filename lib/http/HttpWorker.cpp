#include <x0/http/HttpWorker.h>
#include <x0/http/HttpServer.h>

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
	evNewConnection_(loop_),
	evSuspend_(loop_),
	evResume_(loop_),
	evExit_(loop_),
	fileinfo(loop_)
{
	evNewConnection_.set<HttpWorker, &HttpWorker::onNewConnection>(this);

	evNewConnection_.start();
	evSuspend_.start();
	evResume_.start();
	evExit_.start();
}

HttpWorker::~HttpWorker()
{
}

void HttpWorker::run()
{
	ev_loop(loop_, 0);
}

void HttpWorker::onNewConnection(ev::async& w, int revents)
{
}

void HttpWorker::onSuspend(ev::async& w, int revents)
{
}

void HttpWorker::onResume(ev::async& w, int revents)
{
}

void HttpWorker::onExit(ev::async& w, int revents)
{
}

} // namespace x0
