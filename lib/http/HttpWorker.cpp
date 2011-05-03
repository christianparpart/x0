#include <x0/http/HttpWorker.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpConnection.h>

#include <ext/atomicity.h>
#include <cstdarg>
#include <ev++.h>
#include <signal.h>
#include <pthread.h>

// XXX one a connection has been passed to a worker, it is *bound* to it.

#ifndef NDEBUG
#	define TRACE(msg...) (this->debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

namespace x0 {

unsigned HttpWorker::idpool_ = 0;

HttpWorker::HttpWorker(HttpServer& server, struct ev_loop *loop) :
#ifndef NDEBUG
	Logging("HttpWorker/%d", idpool_),
#endif
	CustomDataMgr(),
	id_(idpool_++),
	server_(server),
	loop_(loop),
	startupTime_(ev_now(loop_)),
	now_(),
	connectionLoad_(0),
	requestLoad_(0),
	requestCount_(0),
	connectionCount_(0),
	thread_(0),
	queue_(),
	queueLock_(),
	connections_(),
	evLoopCheck_(loop_),
	evNewConnection_(loop_),
	evExit_(loop_),
	evExitTimeout_(loop_),
	fileinfo(loop_, &server_.fileinfoConfig_)
{
	evLoopCheck_.set<HttpWorker, &HttpWorker::onLoopCheck>(this);
	evLoopCheck_.start();
	ev_unref(loop_);

	evNewConnection_.set<HttpWorker, &HttpWorker::onNewConnection>(this);
	evNewConnection_.start();
	ev_unref(loop_);

	evExit_.set<HttpWorker, &HttpWorker::onExit>(this);
	evExit_.start();
	ev_unref(loop_);

	evExitTimeout_.set<HttpWorker, &HttpWorker::onExitTimeout>(this);

	pthread_spin_init(&queueLock_, PTHREAD_PROCESS_PRIVATE);

	TRACE("spawned");
}

HttpWorker::~HttpWorker()
{
	TRACE("destroying");

	clearCustomData();
	pthread_spin_destroy(&queueLock_);
}

void HttpWorker::run()
{
	// XXX invoke onWorkerSpawned-hook here because we want to ensure this hook is 
	// XXX being invoked from *within* the worker-thread.
	server_.onWorkerSpawn(this);

	TRACE("enter loop");
	ev_loop(loop_, 0);

	if (evExitTimeout_.is_active()) {
		ev_ref(loop_);
		evExitTimeout_.stop();
	}

	TRACE("event loop left. killing remaining connections.");
	if (!connections_.empty()) {
		auto copy = connections_;
		for (auto c: copy) {
			c->abort();
		}

#ifndef NDEBUG
		for (auto i: connections_) {
			i->debug("connection still open");
		}
#endif
	}

	server_.onWorkerUnspawn(this);
}

void HttpWorker::log(Severity s, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	char buf[512];
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	server_.log(s, "Worker/%d: %s", id_, buf);
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

/** callback to be invoked when new connection(s) have been assigned to this worker.
 */
void HttpWorker::onNewConnection(ev::async& /*w*/, int /*revents*/)
{
	pthread_spin_lock(&queueLock_);
	while (!queue_.empty()) {
		std::pair<int, HttpListener *> client(queue_.front());
		queue_.pop_front();

		pthread_spin_unlock(&queueLock_);

		spawnConnection(client.first, client.second);

		pthread_spin_lock(&queueLock_);
	}
	pthread_spin_unlock(&queueLock_);
}

void HttpWorker::spawnConnection(int fd, HttpListener* listener)
{
	TRACE("client connected; fd:%d", fd);

	HttpConnection* c = new HttpConnection(this, connectionCount_/*id*/);

	++connectionLoad_;
	++connectionCount_;

	connections_.push_front(c);
	HttpConnectionList::iterator i = connections_.begin();

	c->start(listener, fd, i);
}

/** releases/unregisters given (and to-be-destroyed) connection from this worker.
 *
 * This decrements the connection-load counter by one.
 */
void HttpWorker::release(const HttpConnectionList::iterator& connection)
{
	--connectionLoad_;

	HttpConnection* c = *connection;
	connections_.erase(connection);
	delete c;
}

void HttpWorker::handleRequest(HttpRequest *r)
{
	server_.onPreProcess(r);
	if (!server_.onHandleRequest_(r))
		r->finish();
}

void HttpWorker::onExit(ev::async& w, int revents)
{
	TRACE("onExit");

	int gracefulShutdownTimeout = 30;

	evExitTimeout_.start(gracefulShutdownTimeout, 0.0);
	ev_unref(loop_);

	ev_ref(loop_);
	evLoopCheck_.stop();

	ev_ref(loop_);
	evNewConnection_.stop();

	ev_ref(loop_);
	evExit_.stop();
}

void HttpWorker::onExitTimeout(ev::timer& w, int revents)
{
	TRACE("onExitTimeout. killing remaining connections.");

	ev_ref(loop_);
	evExitTimeout_.stop();

	if (!connections_.empty()) {
		auto copy = connections_;
		for (auto c: copy) {
			c->abort();
		}

#ifndef NDEBUG
		for (auto i: connections_) {
			i->debug("connection still open");
		}
#endif
	}
}

void HttpWorker::onLoopCheck(ev::check& /*w*/, int /*revents*/)
{
	// update server time
	now_.update(ev_now(loop_));
}

void HttpWorker::setAffinity(int cpu)
{
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);

	TRACE("setAffinity: %d", cpu);

	int rv;
	if (thread_) {
		// set thread affinity
		rv = pthread_setaffinity_np(thread_, sizeof(set), &set);
	} else {
		// set process affinity (main)
		rv = sched_setaffinity(getpid(), sizeof(set), &set);
	}

	if (rv < 0) {
		log(Severity::error, "setAffinity(%d) failed: %s", cpu, strerror(errno));
	}
}

void HttpWorker::stop()
{
	evExit_.send();
}

} // namespace x0
