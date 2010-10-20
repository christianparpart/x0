#ifndef x0_http_HttpWorker_h
#define x0_http_HttpWorker_h (1)

#include <ev++.h>
#include <signal.h>
#include <pthread.h>

#include <x0/io/FileInfoService.h>

namespace x0 {

class HttpServer;

class HttpWorker
{
public:
	struct Task
	{
		int fd;
		sockaddr_in6 saddr;
		socklen_t slen;

		Task() : fd_(-1), saddr(), slen(sizeof(saddr)) {}
		Task(int fd, const sockaddr_in6& sa) : fd(_fd), saddr(sa), slen(sizeof(sa)) {}
	};

private:
	HttpServer& server_;
	struct ev_loop *loop_;
	sig_atomic_t connectionLoad_;
	pthread_t thread_;

	ev::async evNewConnection_;
	ev::async evSuspend_;
	ev::async evResume_;
	ev::async evExit_;

	// list of connections
	// stat() cache

	friend class HttpServer;

public:
	FileInfoService fileinfo;

public:
	HttpWorker(HttpServer& server, struct ev_loop *loop);
	~HttpWorker();

	struct ev_loop *loop() const;
	HttpServer& server() const;

	void enqueue(Task&& task);

protected:
	virtual void run();

	void onNewConnection(ev::async& w, int revents);
	void onSuspend(ev::async& w, int revents);
	void onResume(ev::async& w, int revents);
	void onExit(ev::async& w, int revents);
};

// {{{ inlines
inline struct ev_loop *HttpWorker::loop() const
{
	return loop_;
}

inline HttpServer& HttpWorker::server() const
{
	return server_;
}
// }}}

} // namespace x0

#endif
