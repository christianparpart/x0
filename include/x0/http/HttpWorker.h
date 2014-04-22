/* <HttpWorker.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_http_HttpWorker_h
#define x0_http_HttpWorker_h (1)

#include <x0/Api.h>
#include <x0/http/Types.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpFileMgr.h>
#include <x0/CustomDataMgr.h>
#include <x0/DateTime.h>
#include <x0/Severity.h>
#include <x0/LogMessage.h>
#include <x0/PerformanceCounter.h>
#include <x0/Queue.h>

#include <deque>
#include <list>
#include <atomic>
#include <utility>
#include <functional>
#include <ev++.h>
#include <signal.h>
#include <pthread.h>

namespace x0 {

class Socket;
class ServerSocket;

//! \addtogroup http
//@{

class HttpServer;
class HttpConnection;
class HttpRequest;

//#define X0_WORKER_POST_LIBEV

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
class X0_API HttpWorker
{
    CUSTOMDATA_API_INLINE

public:
    enum State {
        Inactive = 0,
        Running = 1,
        Suspended = 2
    };

    typedef std::list<HttpConnection*> ConnectionList;
    typedef ConnectionList::iterator ConnectionHandle;

private:
    unsigned id_;
    State state_;
    HttpServer& server_;
    ev::loop_ref loop_;
    ev_tstamp startupTime_;
    DateTime now_;
    std::atomic<int> connectionLoad_;
    std::atomic<unsigned long long> requestCount_;
    unsigned long long connectionCount_;
    pthread_t thread_;
    Queue<std::pair<Socket*, ServerSocket*>> queue_;

    pthread_mutex_t resumeLock_;
    pthread_cond_t resumeCondition_;

    PerformanceCounter<15 * 60> performanceCounter_;

    std::list<std::function<void()>> stopHandler_;
    std::list<std::function<void()>> killHandler_;

    HttpConnection* connections_;
    HttpConnection* freeConnections_;

    ev::check evLoopCheck_;
    ev::async evNewConnection_;
    ev::async evWakeup_;

#if !defined(X0_WORKER_POST_LIBEV)
    pthread_mutex_t postLock_;
    std::deque<std::function<void()>> postQueue_;
#endif

    friend class HttpPlugin;
    friend class HttpCore;
    friend class HttpServer;
    friend class HttpConnection;
    friend class HttpRequest;

public:
    HttpFileMgr fileinfo;

public:
    HttpWorker(HttpServer& server, struct ev_loop *loop, unsigned int id, bool threaded);
    ~HttpWorker();

    void setName(const char* fmt, ...);

    ev_tstamp startupTime() const { return startupTime_; }
    ev_tstamp uptime() const { return ev_now(loop_) - startupTime_; }

    const DateTime& now() const;

    unsigned id() const;
    struct ev_loop *loop() const;
    HttpServer& server() const;

    bool isInactive() const { return state_ == Inactive; }
    bool isRunning() const { return state_ == Running; }
    bool isSuspended() const { return state_ == Suspended; }

    template<typename T>
    bool eachConnection(T cb) {
        for (HttpConnection* c = connections_; c != nullptr; c = c->next_)
            if (!cb(c))
                return false;

        return true;
    }

    int connectionLoad() const;
    unsigned long long requestCount() const;
    unsigned long long connectionCount() const;

    void fetchPerformanceCounts(double* p1, double* p5, double* p15) const;

    void enqueue(std::pair<Socket*, ServerSocket*>&& handle);
    void handleRequest(HttpRequest *r);
    void release(HttpConnection* connection);

    template<typename... Args>
    void log(Severity s, const char* fmt, Args... args);

    void log(LogMessage&& msg);

    void setAffinity(int cpu);

    void bind(ServerSocket* s);

    template<class K, void (K::*fn)()>
    void post(K* object);

    template<class K, void (K::*fn)(void*)>
    void post(K* object, void* arg);

    inline void post(const std::function<void()>& callback);

    inline void wakeup();

    void stop();
    void kill();
    void join();

    void suspend();
    void resume();

    std::list<std::function<void()>>::iterator registerStopHandler(std::function<void()> callback);
    void unregisterStopHandler(std::list<std::function<void()>>::iterator handle);

    std::list<std::function<void()>>::iterator registerKillHandler(std::function<void()> callback);
    void unregisterKillHandler(std::list<std::function<void()>>::iterator handle);

    void freeCache();

private:
    template<class K, void (K::*fn)()>
    static void post_thunk(int revents, void* arg);

    template<class K, void (K::*fn)(void*)>
    static void post_thunk2(int revents, void* arg);

    static void post_thunk3(int revents, void* arg);

    void run();

    void onLoopCheck(ev::check& w, int revents);
    void onNewConnection(ev::async& w, int revents);
    void onWakeup(ev::async& w, int revents);
    void spawnConnection(Socket* client, ServerSocket* listener);
    static void* _run(void*);
    void _stop();
    void _kill();
    void _suspend();
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

inline const DateTime& HttpWorker::now() const
{
    return now_;
}

inline int HttpWorker::connectionLoad() const
{
    return connectionLoad_;
}

inline unsigned long long HttpWorker::requestCount() const
{
    return requestCount_;
}

inline unsigned long long HttpWorker::connectionCount() const
{
    return connectionCount_;
}

/*! Invokes given callback within this worker's thread.
 *
 * \param object this-pointer to the target object
 */
template<class K, void (K::*fn)()>
void HttpWorker::post(K* object)
{
#if !defined(X0_WORKER_POST_LIBEV)
    pthread_mutex_lock(&postLock_);
    postQueue_.push_back(std::bind(fn, object));
    pthread_mutex_unlock(&postLock_);
#else
    ev_once(loop_, /*fd*/ -1, /*events*/ 0, /*timeout*/ 0, &post_thunk<K, fn>, object);
#endif
    evWakeup_.send();
}

template<class K, void (K::*fn)()>
void HttpWorker::post_thunk(int revents, void* arg)
{
    (static_cast<K *>(arg)->*fn)();
}

/*! Invokes given callback within this worker's thread.
 *
 * \param object this-pointer to the target object
 * \param arg custom private pointer, passed as argument to the objects member function
 */
template<class K, void (K::*fn)(void*)>
void HttpWorker::post(K* object, void* arg)
{
#if !defined(X0_WORKER_POST_LIBEV)
    pthread_mutex_lock(&postLock_);
    postQueue_.push_back(std::bind(fn, object, arg));
    pthread_mutex_unlock(&postLock_);
#else
    auto priv = std::make_pair(object, arg);
    ev_once(loop_, /*fd*/ -1, /*events*/ 0, /*timeout*/ 0, &post_thunk2<K, fn>, priv);
#endif
    evWakeup_.send();
}

template<class K, void (K::*fn)(void*)>
void HttpWorker::post_thunk2(int revents, void* arg)
{
    auto priv = (std::pair<K*, void*>) arg;

    try {
        (static_cast<K *>(priv->first)->*fn)(priv->second);
    } catch (...) {
        delete priv;
        throw;
    }

    delete priv;
}

inline void HttpWorker::post(const std::function<void()>& callback)
{
#if !defined(X0_WORKER_POST_LIBEV)
    pthread_mutex_lock(&postLock_);
    postQueue_.push_back(callback);
    pthread_mutex_unlock(&postLock_);
#else
    auto p = new std::function<void()>(callback);
    ev_once(loop_, /*fd*/ -1, /*events*/ 0, /*timeout*/ 0, &HttpWorker::post_thunk3, (void*)p);
#endif
    evWakeup_.send();
}

inline void HttpWorker::wakeup()
{
    evWakeup_.send();
}

inline void HttpWorker::fetchPerformanceCounts(double* p1, double* p5, double* p15) const
{
    *p1 += performanceCounter_.average(60 * 1);
    *p5 += performanceCounter_.average(60 * 5);
    *p15 += performanceCounter_.average(60 * 15);
}

template<typename... Args>
inline void HttpWorker::log(Severity s, const char* fmt, Args... args)
{
    log(LogMessage(s, fmt, args...));
}
// }}}

} // namespace x0

#endif
