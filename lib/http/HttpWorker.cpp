/* <src/HttpWorker.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/http/HttpWorker.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpConnection.h>
#include <x0/ServerSocket.h>

#include <algorithm>
#include <cstdarg>
#include <ev++.h>
#include <signal.h>
#include <pthread.h>

// XXX one a connection has been passed to a worker, it is *bound* to it.

#if 0 // !defined(NDEBUG)
#	define TRACE(msg...) DEBUG("HttpWorker: " msg)
#else
#	define TRACE(msg...) do {} while (0)
#endif

namespace x0 {

/*!
 * Holds map of spawned workers by thread ID.
 */
static std::map<pthread_t, HttpWorker*> workers;

/*!
 * Creates an HTTP worker instance.
 *
 * \param server   the worker parents server instance
 * \param loop     the event loop to be used within this worker
 * \param id       unique ID within the server instance
 * \param threaded whether or not to spawn a thread to actually run this worker
 */
HttpWorker::HttpWorker(HttpServer& server, struct ev_loop *loop, unsigned int id, bool threaded) :
	id_(id),
	state_(Inactive),
	server_(server),
	loop_(loop),
	startupTime_(ev_now(loop_)),
	now_(),
	connectionLoad_(0),
	requestCount_(0),
	connectionCount_(0),
	thread_(pthread_self()),
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
	evWakeup_(loop_),
	fileinfo(loop_, &server_.fileinfoConfig_)
{
	evLoopCheck_.set<HttpWorker, &HttpWorker::onLoopCheck>(this);
	evLoopCheck_.start();

	evNewConnection_.set<HttpWorker, &HttpWorker::onNewConnection>(this);
	evNewConnection_.start();

	evWakeup_.set<&HttpWorker::onWakeup>();
	evWakeup_.start();

	pthread_spin_init(&queueLock_, PTHREAD_PROCESS_PRIVATE);

	pthread_mutex_init(&resumeLock_, nullptr);
	pthread_cond_init(&resumeCondition_, nullptr);

	if (threaded) {
		pthread_create(&thread_, nullptr, &HttpWorker::_run, this);
	}

	TRACE("spawned");
}

HttpWorker::~HttpWorker()
{
	TRACE("destroying");

	clearCustomData();

	pthread_cond_destroy(&resumeCondition_);
	pthread_mutex_destroy(&resumeLock_);

	pthread_spin_destroy(&queueLock_);

	evLoopCheck_.stop();
	evNewConnection_.stop();
	evWakeup_.stop();

	ev_loop_destroy(loop_);
}

void* HttpWorker::_run(void* p)
{
	reinterpret_cast<HttpWorker*>(p)->run();
	return nullptr;
}

/**
 * Points to the current's worker thread.
 */
HttpWorker* HttpWorker::current()
{
	auto i = workers.find(pthread_self());
	return i != workers.end() ? i->second : nullptr;
}

/**
 * Returns a numerical ID of the current worker thread or -1 if not found.
 */
int HttpWorker::currentId()
{
	HttpWorker* currentWorker = current();
	return currentWorker ? static_cast<int>(currentWorker->id()) : -1;
}

void HttpWorker::run()
{
	workers[pthread_self()] = this;

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

	auto i = workers.find(pthread_self());
	if (i != workers.end()) {
		workers.erase(i);
	} else {
		TRACE("warning: potential bug in HttpWorker::current(). I couldn't find myself.");
	}
}

void HttpWorker::log(LogMessage&& msg)
{
	msg.addTag("%u", id());

	server().log(std::forward<LogMessage>(msg));
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

void HttpWorker::onWakeup(ev::async& w, int revents)
{
	// no-op - this callback is simply used to wake up the worker's event loop
}

void HttpWorker::spawnConnection(Socket* client, ServerSocket* listener)
{
	TRACE("client connected; fd:%d", client->handle());

	++connectionLoad_;
	++connectionCount_;

	// XXX since socket has not been used so far, I might be able to defer its creation out of its socket descriptor
	// XXX so that I do not have to double-initialize libev's loop handles for this socket.
	client->setLoop(loop_);

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
	++requestCount_;
	performanceCounter_.touch(now_.value());

#if X0_HTTP_STRICT
	BufferRef expectHeader = r->requestHeader("Expect");
	bool contentRequired = r->method == "POST" || r->method == "PUT";

	if (contentRequired) {
		if (r->connection.contentLength() == -1 && !r->connection.isChunked()) {
			r>status = HttpStatus::LengthRequired;
			r->finish();
			return true;
		}
	} else {
		if (r->contentAvailable()) {
			r->status = HttpStatus::BadRequest; // FIXME do we have a better status code?
			r->finish();
			return true;
		}
	}

	if (expectHeader) {
		r->expectingContinue = equals(expectHeader, "100-continue");

		if (!r->expectingContinue || !r->supportsProtocol(1, 1)) {
			r->status = HttpStatus::ExpectationFailed;
			r->finish();
			return true;
		}
	}
#endif

	server_.onPreProcess(r);

	if (!server_.onHandleRequest_(r))
		r->finish();
}

void HttpWorker::_stop()
{
	TRACE("_stop");

	evLoopCheck_.stop();
	evNewConnection_.stop();
	evWakeup_.stop();

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

	int rv = pthread_setaffinity_np(thread_, sizeof(set), &set);
	if (rv < 0) {
		log(Severity::error, "setting scheduler affinity on CPU %d failed for worker %u. %s",
			cpu, id_, strerror(errno));
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
	TRACE("stop: post -> _stop()");
	post<HttpWorker, &HttpWorker::_stop>(this);
}

void HttpWorker::join()
{
	if (thread_ != pthread_self()) {
		pthread_join(thread_, nullptr);
	}
}

/*! Actually aborts all active connections.
 *
 * \see HttpConnection::abort()
 */
void HttpWorker::kill()
{
	TRACE("kill: post -> _kill()");
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
	TRACE("_kill()");
	if (!connections_.empty()) {
		auto copy = connections_;

		for (auto c: copy)
			c->abort();

#ifndef NDEBUG
		for (auto i: connections_)
			i->log(Severity::debug, "connection still open");
#endif
	}

	for (auto handler: killHandler_) {
		TRACE("_kill: invoke kill handler");
		handler();
	}
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

void HttpWorker::post_thunk3(int revents, void* arg)
{
	std::function<void()>* callback = static_cast<std::function<void()>*>(arg);
	(*callback)();
	delete callback;
}

} // namespace x0
