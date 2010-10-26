#ifndef x0_http_HttpWorker_h
#define x0_http_HttpWorker_h (1)

#include <x0/io/FileInfoService.h>
#include <x0/AtomicInt.h>
#include <x0/DateTime.h>
#include <x0/Severity.h>

#include <deque>
#include <ev++.h>
#include <signal.h>
#include <pthread.h>

namespace x0 {

class HttpServer;
class HttpConnection;

class HttpWorker
{
public:
	enum State
	{
		Active,
		Inactive,
		Exiting
	};

private:
	static unsigned idpool_;

	unsigned id_;
	HttpServer& server_;
	struct ev_loop *loop_;
	DateTime now_;
	AtomicInt connectionLoad_;
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

public:
	FileInfoService fileinfo;

public:
	HttpWorker(HttpServer& server, struct ev_loop *loop);
	~HttpWorker();

	const DateTime& now() const;

	unsigned id() const;
	struct ev_loop *loop() const;
	HttpServer& server() const;
	State state() const;

	int load() const;

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

inline int HttpWorker::load() const
{
	return connectionLoad_;
}
// }}}

} // namespace x0

#endif
