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
	now_(),
	connectionLoad_(0),
	thread_(),
	state_(Active),
	queue_(),
	evLoopCheck_(loop_),
	evNewConnection_(loop_),
	evSuspend_(loop_),
	evResume_(loop_),
	evExit_(loop_),
	fileinfo(loop_, &server_.fileinfoConfig_)
{
	evLoopCheck_.set<HttpWorker, &HttpWorker::onLoopCheck>(this);
	evLoopCheck_.start();

	evNewConnection_.set<HttpWorker, &HttpWorker::onNewConnection>(this);
	evNewConnection_.start();
	ev_unref(loop_);

	evSuspend_.set<HttpWorker, &HttpWorker::onSuspend>(this);
	evSuspend_.start();
	ev_unref(loop_);

	evResume_.set<HttpWorker, &HttpWorker::onResume>(this);
	evResume_.start();
	ev_unref(loop_);

	evExit_.set<HttpWorker, &HttpWorker::onExit>(this);
	evExit_.start();

#if !defined(NO_BUGGY_EVXX)
	// libev's ev++ (at least till version 3.80) does not initialize `sent` to zero)
	ev_async_set(&evNewConnection_);
	ev_async_set(&evSuspend_);
	ev_async_set(&evResume_);
	ev_async_set(&evExit_);
#endif

	pthread_spin_init(&queueLock_, PTHREAD_PROCESS_PRIVATE);
}

HttpWorker::~HttpWorker()
{
	pthread_spin_destroy(&queueLock_);
}

void HttpWorker::run()
{
	while (state_ != Exiting)
	{
		printf("HttpWorker/%d enter loop\n", id_);
		ev_loop(loop_, 0);
	}
}

/** enqueues/assigns/registers given client connection information to this worker.
 */
void HttpWorker::enqueue(std::pair<int, HttpListener *>&& client)
{
	pthread_spin_lock(&queueLock_);
	queue_.push_back(client);
	evNewConnection_.send();
	pthread_spin_unlock(&queueLock_);
}

/** releases/unregisters given (and to-be-destroyed) connection from this worker.
 *
 * This decrements the connection-load counter by one.
 */
void HttpWorker::release(HttpConnection *)
{
	pthread_spin_lock(&queueLock_);
	--connectionLoad_;
	pthread_spin_unlock(&queueLock_);
}

unsigned HttpWorker::load() const
{
	// XXX  I am reusing queueLock for the connection-count variable as they're tight coupled.
	// TODO Change this if it results into bad performance impacts :)

	pthread_spin_lock(&queueLock_);
	unsigned result = connectionLoad_;
	pthread_spin_unlock(&queueLock_);

	return result;
}

/** callback to be invoked when new connection(s) have been assigned to this worker.
 */
void HttpWorker::onNewConnection(ev::async& /*w*/, int /*revents*/)
{
	while (!queue_.empty())
	{
		pthread_spin_lock(&queueLock_);
		std::pair<int, HttpListener *> client(queue_.front());
		queue_.pop_front();
		++connectionLoad_;
		pthread_spin_unlock(&queueLock_);

		printf("HttpWorker/%d client connected; fd:%d\n", id_, client.first);

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
	printf("HttpWorker/%d onExit\n", id_);
	ev_ref(loop_);
	evNewConnection_.stop();

	ev_ref(loop_);
	evSuspend_.stop();

	ev_ref(loop_);
	evResume_.stop();

	evExit_.stop();

	state_ = Exiting;
}

void HttpWorker::onLoopCheck(ev::check& /*w*/, int /*revents*/)
{
	// update server time
	now_.update(static_cast<time_t>(ev_now(loop_)));
}

} // namespace x0
