#include <x0/http/HttpWorker.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpConnection.h>
#include <x0/ServerSocket.h>

#include <algorithm>
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
	state_(Inactive),
	server_(server),
	loop_(loop),
	startupTime_(ev_now(loop_)),
	now_(),
	connectionLoad_(0),
	requestCount_(0),
	connectionCount_(0),
	thread_(0),
	queue_(),
	queueLock_(),
	resumeLock_(),
	resumeCondition_(),
	performanceCounter_(),
	stopHandler_(),
	killHandler_(),
	connections_(),
	evLoopCheck_(loop_),
	evNewConnection_(loop_),
	fileinfo(loop_, &server_.fileinfoConfig_)
{
	evLoopCheck_.set<HttpWorker, &HttpWorker::onLoopCheck>(this);
	evLoopCheck_.start();
	ev_unref(loop_);

	evNewConnection_.set<HttpWorker, &HttpWorker::onNewConnection>(this);
	evNewConnection_.start();
	ev_unref(loop_);

	pthread_spin_init(&queueLock_, PTHREAD_PROCESS_PRIVATE);

	pthread_mutex_init(&resumeLock_, nullptr);
	pthread_cond_init(&resumeCondition_, nullptr);

	TRACE("spawned");
}

HttpWorker::~HttpWorker()
{
	TRACE("destroying");

	clearCustomData();

	pthread_cond_destroy(&resumeCondition_);
	pthread_mutex_destroy(&resumeLock_);

	pthread_spin_destroy(&queueLock_);

	if (evLoopCheck_.is_active()) {
		ev_ref(loop_);
		evLoopCheck_.stop();
	}

	if (evNewConnection_.is_active()) {
		ev_ref(loop_);
		evNewConnection_.stop();
	}
}

void HttpWorker::run()
{
	state_ = Running;

	// XXX invoke onWorkerSpawned-hook here because we want to ensure this hook is 
	// XXX being invoked from *within* the worker-thread.
	server_.onWorkerSpawn(this);

	TRACE("enter loop");
	ev_loop(loop_, 0);

	TRACE("event loop left. killing remaining connections (%ld).", connections_.size());
	if (!connections_.empty())
		_kill();

	server_.onWorkerUnspawn(this);

	state_ = Inactive;
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
void HttpWorker::enqueue(std::pair<Socket*, ServerSocket*>&& client)
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
		std::pair<Socket*, ServerSocket*> client(queue_.front());
		queue_.pop_front();

		pthread_spin_unlock(&queueLock_);

		spawnConnection(client.first, client.second);

		pthread_spin_lock(&queueLock_);
	}
	pthread_spin_unlock(&queueLock_);
}

void HttpWorker::spawnConnection(Socket* client, ServerSocket* listener)
{
	TRACE("client connected; fd:%d", client->handle());

	++connectionLoad_;
	++connectionCount_;

	HttpConnection* c = new HttpConnection(this, connectionCount_/*id*/);

	connections_.push_front(c);
	ConnectionHandle i = connections_.begin();

	c->start(listener, client, i);
}

/** releases/unregisters given (and to-be-destroyed) connection from this worker.
 *
 * This decrements the connection-load counter by one.
 */
void HttpWorker::release(const ConnectionHandle& connection)
{
	--connectionLoad_;

	HttpConnection* c = *connection;
	connections_.erase(connection);
	delete c;
}

void HttpWorker::handleRequest(HttpRequest *r)
{
	performanceCounter_.touch(now_.value());

	server_.onPreProcess(r);
	if (!server_.onHandleRequest_(r))
		r->finish();
}

void HttpWorker::_stop()
{
	TRACE("_stop");

	ev_ref(loop_);
	evLoopCheck_.stop();

	ev_ref(loop_);
	evNewConnection_.stop();

	for (auto handler: stopHandler_)
		handler();
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

/** suspend the execution of the worker thread until resume() is invoked.
 *
 * \note has no effect on main worker
 * \see resume()
 */
void HttpWorker::suspend()
{
	TRACE("suspend");

	if (id_ != 0)
		post<HttpWorker, &HttpWorker::_suspend>(this);
}

void HttpWorker::_suspend()
{
	TRACE("_suspend");
	pthread_mutex_lock(&resumeLock_);
	state_ = Suspended;
	pthread_cond_wait(&resumeCondition_, &resumeLock_);
	state_ = Running;
	pthread_mutex_unlock(&resumeLock_);
}

/** resumes the previousely suspended worker thread.
 *
 * \note has no effect on main worker
 * \see suspend()
 */
void HttpWorker::resume()
{
	TRACE("resume");
	if (id_ != 0)
		pthread_cond_signal(&resumeCondition_);
}

void HttpWorker::stop()
{
	post<HttpWorker, &HttpWorker::_stop>(this);
}

/*! Actually aborts all active connections.
 *
 * \see HttpConnection::abort()
 */
void HttpWorker::kill()
{
	post<HttpWorker, &HttpWorker::_kill>(this);
}

/*! Actually aborts all active connections.
 *
 * \note Must be invoked from within the worker's thread.
 *
 * \see HttpConnection::abort()
 */
void HttpWorker::_kill()
{
	if (!connections_.empty()) {
		auto copy = connections_;

		for (auto c: copy)
			c->abort();

#ifndef NDEBUG
		for (auto i: connections_)
			i->debug("connection still open");
#endif
	}

	for (auto handler: killHandler_)
		handler();
}

std::list<std::function<void()>>::iterator HttpWorker::registerStopHandler(std::function<void()> callback)
{
	stopHandler_.push_front(callback);
	return stopHandler_.begin();
}

void HttpWorker::unregisterStopHandler(std::list<std::function<void()>>::iterator handle)
{
	stopHandler_.erase(handle);
}

std::list<std::function<void()>>::iterator HttpWorker::registerKillHandler(std::function<void()> callback)
{
	killHandler_.push_front(callback);
	return killHandler_.begin();
}

void HttpWorker::unregisterKillHandler(std::list<std::function<void()>>::iterator handle)
{
	killHandler_.erase(handle);
}

} // namespace x0
