#ifndef x0_http_HttpWorker_h
#define x0_http_HttpWorker_h (1)

#include <x0/io/FileInfoService.h>

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
	sig_atomic_t connectionLoad_;
	pthread_t thread_;
	State state_;
	std::deque<std::pair<int, HttpListener *> > queue_;

	ev::async evNewConnection_;
	ev::async evSuspend_;
	ev::async evResume_;
	ev::async evExit_;

	// list of connections
	// stat() cache

	friend class HttpServer;
	friend class HttpConnection;

public:
	FileInfoService fileinfo;

public:
	HttpWorker(HttpServer& server, struct ev_loop *loop);
	~HttpWorker();

	unsigned id() const;
	struct ev_loop *loop() const;
	HttpServer& server() const;
	State state() const;

	unsigned load() const;

	void enqueue(std::pair<int, HttpListener *>&& handle);

protected:
	virtual void run();

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
// }}}

} // namespace x0

#endif
