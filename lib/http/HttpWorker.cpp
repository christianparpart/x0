#include <x0/http/HttpWorker.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpConnection.h>

#include <ev++.h>
#include <signal.h>
#include <pthread.h>

// XXX one a connection has been passed to a worker, it is *bound* to it.

namespace x0 {

unsigned HttpWorker::idpool_ = 0;

HttpWorker::HttpWorker(HttpServer& server, struct ev_loop *loop) :
	id_(idpool_++),
	server_(server),
	loop_(loop),
	connectionLoad_(0),
	thread_(),
	state_(Active),
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

#if !defined(NO_BUGGY_EVXX)
	// libev's ev++ (at least till version 3.80) does not initialize `sent` to zero)
	ev_async_set(&evNewConnection_);
	ev_async_set(&evSuspend_);
	ev_async_set(&evResume_);
	ev_async_set(&evExit_);
#endif
}

HttpWorker::~HttpWorker()
{
}

unsigned HttpWorker::load() const
{
	return connectionLoad_;
}

void HttpWorker::run()
{
	while (state_ != Exiting)
	{
		ev_loop(loop_, EVLOOP_ONESHOT);
	}
}

void HttpWorker::enqueue(std::pair<int, HttpListener *>&& client)
{
	queue_.push_back(client);
	evNewConnection_.send();
}

void HttpWorker::onNewConnection(ev::async& w, int revents)
{
	while (!queue_.empty())
	{
		std::pair<int, HttpListener *> client(queue_.front());
		queue_.pop_front();

		printf("HttpWorker/%d client connected; fd:%d\n", id_, client.first);

		++connectionLoad_;
		HttpConnection *conn = new HttpConnection(*client.second, *this, client.first);

		if (conn->isClosed())
			delete conn;
		else
			conn->start();
	}
}

void HttpWorker::onSuspend(ev::async& w, int revents)
{
	state_ = Inactive;
}

void HttpWorker::onResume(ev::async& w, int revents)
{
	state_ = Active;
}

void HttpWorker::onExit(ev::async& w, int revents)
{
	state_ = Exiting;
}

} // namespace x0
