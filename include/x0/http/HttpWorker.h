#ifndef x0_http_HttpWorker_h
#define x0_http_HttpWorker_h (1)

#include <x0/io/FileInfoService.h>
#include <x0/Logging.h>
#include <x0/CustomDataMgr.h>
#include <x0/DateTime.h>
#include <x0/Severity.h>

#include <deque>
#include <atomic>
#include <ev++.h>
#include <signal.h>
#include <pthread.h>

namespace x0 {

//! \addtogroup http
//@{

class HttpServer;
class HttpConnection;

/**
 * \brief thread-local worker.
 *
 * the HTTP server may spawn multiple workers (one per thread) to
 * improve scalability across multiple CPUs/cores.
 * This class aims to make make some resources lock-free by providing
 * each thread with its own instanciation (e.g. the stat()-cache).
 *
 * A single connection is served by a single worker to ensure that
 * the plugins accessing the stat()-cache and all (other) CustomDataMgr
 * instances to always get the data they expect.
 *
 * \see HttpServer, CustomDataMgr
 */
class HttpWorker :
#ifndef NDEBUG
	public Logging,
#endif
	public CustomDataMgr
{
public:
	enum State {
		Active,
		Inactive,
		Exiting
	};

private:
	static unsigned idpool_;

	unsigned id_;
	HttpServer& server_;
	struct ev_loop *loop_;
	ev_tstamp startupTime_;
	DateTime now_;
	std::atomic<int> connectionLoad_;
	std::atomic<int> requestLoad_;
	std::atomic<unsigned long long> requestCount_;
	pthread_t thread_;
	State state_;
	std::deque<std::pair<int, HttpListener *> > queue_;
	mutable pthread_spinlock_t queueLock_;

	ev::check evLoopCheck_;
	ev::async evNewConnection_;
	ev::async evSuspend_;
	ev::async evResume_;
	ev::async evExit_;

	friend class HttpPlugin;
	friend class HttpCore;
	friend class HttpServer;
	friend class HttpConnection;
	friend class HttpRequest;

public:
	FileInfoService fileinfo;

public:
	HttpWorker(HttpServer& server, struct ev_loop *loop);
	~HttpWorker();

	ev_tstamp startupTime() const { return startupTime_; }
	ev_tstamp uptime() const { return ev_now(loop_) - startupTime_; }

	const DateTime& now() const;

	unsigned id() const;
	struct ev_loop *loop() const;
	HttpServer& server() const;
	State state() const;

	int connectionLoad() const;
	int requestLoad() const;
	unsigned long long requestCount() const;

	void enqueue(std::pair<int, HttpListener *>&& handle);
	void handleRequest(HttpRequest *r);
	void release(HttpConnection *connection);

	void log(Severity s, const char *fmt, ...);

	void setAffinity(int cpu);

protected:
	virtual void run();

	void onLoopCheck(ev::check& w, int revents);
	void onNewConnection(ev::async& w, int revents);
	void onSuspend(ev::async& w, int revents);
	void onResume(ev::async& w, int revents);
	void onExit(ev::async& w, int revents);
};

//@}

// {{{ inlines
inline unsigned HttpWorker::id() const
{
	return id_;
}

inline struct ev_loop *HttpWorker::loop() const
{
	return loop_;
}

inline HttpServer& HttpWorker::server() const
{
	return server_;
}

inline HttpWorker::State HttpWorker::state() const
{
	return state_;
}

inline const DateTime& HttpWorker::now() const
{
	return now_;
}

inline int HttpWorker::connectionLoad() const
{
	return connectionLoad_;
}

inline int HttpWorker::requestLoad() const
{
	return requestLoad_;
}

inline unsigned long long HttpWorker::requestCount() const
{
	return requestCount_;
}
// }}}

} // namespace x0

#endif
