/* <x0/HttpServer.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpWorker.h>
#include <x0/ServerSocket.h>
#include <x0/SocketSpec.h>
#include <x0/Logger.h>
#include <x0/DebugLogger.h>
#include <x0/Library.h>
#include <x0/AnsiColor.h>
#include <x0/strutils.h>
#include <x0/sysconfig.h>

#include <sd-daemon.h>

#include <iostream>
#include <fstream>
#include <cstdarg>
#include <cstdlib>

#if defined(HAVE_SYS_UTSNAME_H)
#	include <sys/utsname.h>
#endif

#if defined(HAVE_ZLIB_H)
#	include <zlib.h>
#endif

#if defined(HAVE_BZLIB_H)
#	include <bzlib.h>
#endif

#include <sys/resource.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <stdio.h>

#if 0 // !defined(XZERO_NDEBUG)
#	define TRACE(msg...) DEBUG("HttpServer: " msg)
#else
#	define TRACE(msg...) do {} while (0)
#endif


namespace x0 {

/** initializes the HTTP server object.
 * \param io_service an Asio io_service to use or nullptr to create our own one.
 * \see HttpServer::run()
 */
HttpServer::HttpServer(struct ::ev_loop *loop, unsigned generation) :
    requestHandler(),

    onConnectionOpen(),
    onPreProcess(),
    onResolveDocumentRoot(),
    onResolveEntity(),
    onPostProcess(),
    onRequestDone(),
    onConnectionClose(),
    onConnectionStateChanged(),
    onWorkerSpawn(),
    onWorkerUnspawn(),

    generation_(generation),

    listeners_(),
    loop_(loop ? loop : ev_default_loop(0)),
    startupTime_(ev_now(loop_)),
    logger_(),
    logLevel_(Severity::warn),
    colored_log_(false),
    workerIdPool_(0),
    workers_(),
    lastWorker_(0),
    maxConnections(512),
    maxKeepAlive(TimeSpan::fromSeconds(60)),
    maxKeepAliveRequests(100),
    maxReadIdle(TimeSpan::fromSeconds(60)),
    maxWriteIdle(TimeSpan::fromSeconds(360)),
    tcpCork(false),
    tcpNoDelay(false),
    lingering(TimeSpan::Zero),
    tag("x0/" VERSION),
    advertise(true),
    maxRequestUriSize(4 * 1024),
    maxRequestHeaderSize(1 * 1024),
    maxRequestHeaderCount(100),
    maxRequestBodySize(4 * 1024 * 1024),
    requestHeaderBufferSize(8 * 1024),
    requestBodyBufferSize(8 * 1024)
{
    DebugLogger::get().onLogWrite = [&](const char* msg, size_t n) {
        LogMessage lm(Severity::debug1, "%s", msg);
        logger_->write(lm);
    };

    HttpRequest::initialize();

    logger_.reset(new FileLogger(STDERR_FILENO, [this]() {
        return static_cast<time_t>(ev_now(loop_));
    }));

    // setting a reasonable default max-connection limit.
    // However, this cannot be computed as we do not know what the user
    // actually configures, such as fastcgi requires +1 fd, local file +1 fd,
    // http client connection of course +1 fd, listener sockets, etc.
    struct rlimit rlim;
    if (::getrlimit(RLIMIT_NOFILE, &rlim) == 0)
        maxConnections = std::max(int(rlim.rlim_cur / 3) - 5, 1);

    // Spawn main-thread worker
    spawnWorker();
}

HttpServer::~HttpServer()
{
    TRACE("destroying");
    stop();

    for (auto i: listeners_)
        delete i;

    while (!workers_.empty())
        destroyWorker(workers_[workers_.size() - 1]);

#ifndef __APPLE__
    // explicit cleanup
    DebugLogger::get().reset();
#endif
}

void HttpServer::onNewConnection(std::unique_ptr<Socket>&& cs, ServerSocket* ss)
{
    if (cs) {
        selectWorker()->enqueue(std::make_pair(std::move(cs), ss));
    } else {
        log(Severity::error, "Accepting incoming connection failed. %s", strerror(errno));
    }
}

// {{{ worker mgnt
HttpWorker *HttpServer::spawnWorker()
{
    bool isMainWorker = workers_.empty();

    HttpWorker *worker = new HttpWorker(*this, isMainWorker ? loop_ : nullptr, workerIdPool_++, !isMainWorker);

    workers_.push_back(worker);

    return worker;
}

/**
 * Selects (by round-robin) the next worker.
 *
 * \return a worker
 * \note This method is not thread-safe, and thus, should not be invoked within a request handler.
 */
HttpWorker* HttpServer::nextWorker()
{
    // select by RR (round-robin)
    // this is thread-safe since only one thread is to select a new worker
    // (the main thread)

    if (++lastWorker_ == workers_.size())
        lastWorker_ = 0;

    return workers_[lastWorker_];
}

HttpWorker *HttpServer::selectWorker()
{
#if 1 // defined(X0_WORKER_RR)
    return nextWorker();
#else
    // select by lowest connection load
    HttpWorker *best = workers_[0];
    int value = best->connectionLoad();

    for (size_t i = 1, e = workers_.size(); i != e; ++i) {
        HttpWorker *w = workers_[i];
        int l = w->connectionLoad();
        if (l < value) {
            value = l;
            best = w;
        }
    }

    return best;
#endif
}

void HttpServer::destroyWorker(HttpWorker *worker)
{
    auto i = std::find(workers_.begin(), workers_.end(), worker);
    assert(i != workers_.end());

    worker->stop();

    if (worker != workers_.front())
        worker->join();

    workers_.erase(i);
    delete worker;
}
// }}}

/** calls run on the internally referenced io_service.
 * \note use this if you do not have your own main loop.
 * \note automatically starts the server if it wasn't started via \p start() yet.
 * \see start(), stop()
 */
int HttpServer::run()
{
    workers_.front()->run();

    return 0;
}

/** unregisters all listeners from the underlying io_service and calls stop on it.
 * \see start(), run()
 */
void HttpServer::stop()
{
    for (auto listener: listeners_)
        listener->stop();

    for (auto worker: workers_)
        worker->stop();
}

void HttpServer::kill()
{
    stop();

    for (auto worker: workers_)
        worker->kill();
}

void HttpServer::log(LogMessage&& msg)
{
    if (logger_) {
#if !defined(XZERO_NDEBUG)
#ifndef __APPLE__
        if (msg.isDebug() && msg.hasTags() && DebugLogger::get().isConfigured()) {
            int level = 3 - msg.severity(); // compute proper debug level
            Buffer text;
            text << msg;
            BufferRef tag = msg.tagAt(msg.tagCount() - 1);
            size_t i = tag.find('/');
            if (i != tag.npos)
                tag = tag.ref(0, i);

            DebugLogger::get().logUntagged(tag.str(), level, "%s", text.c_str());
            return;
        }
#endif
#endif
        logger_->write(msg);
    }
}

/**
 * sets up a TCP/IP ServerSocket on given bind_address and port.
 *
 * If there is already a ServerSocket on this bind_address:port pair
 * then no error will be raised.
 */
ServerSocket* HttpServer::setupListener(const std::string& bind_address, int port, int backlog)
{
    return setupListener(SocketSpec::fromInet(IPAddress(bind_address), port, backlog));
}

ServerSocket *HttpServer::setupUnixListener(const std::string& path, int backlog)
{
    return setupListener(SocketSpec::fromLocal(path, backlog));
}

namespace {
    static inline Buffer readFile(const char* path)
    {
        FILE* fp = fopen(path, "r");
        if (!fp)
            return Buffer();

        Buffer result;
        char buf[4096];

        while (!feof(fp)) {
            size_t n = fread(buf, 1, sizeof(buf), fp);
            result.push_back(buf, n);
        }

        fclose(fp);

        return result;
    }
}

ServerSocket* HttpServer::setupListener(const SocketSpec& _spec)
{
    // validate backlog against system's hard limit
    SocketSpec spec(_spec);
    int somaxconn = readFile("/proc/sys/net/core/somaxconn").toInt();
    if (spec.backlog() > 0) {
        if (somaxconn && spec.backlog() > somaxconn) {
            log(Severity::error,
                "Listener %s configured with a backlog higher than the system permits (%ld > %ld). "
                "See /proc/sys/net/core/somaxconn for your system limits.",
                spec.str().c_str(), spec.backlog(), somaxconn);

            return nullptr;
        }
    }

    // create a new listener
    ServerSocket* lp = new ServerSocket(loop_);
    lp->set<HttpServer, &HttpServer::onNewConnection>(this);

    listeners_.push_back(lp);

    if (spec.backlog() <= 0)
        lp->setBacklog(somaxconn);

    if (spec.multiAcceptCount() > 0)
        lp->setMultiAcceptCount(spec.multiAcceptCount());

    if (lp->open(spec, O_NONBLOCK | O_CLOEXEC)) {
        log(Severity::info, "Listening on %s", spec.str().c_str());
        return lp;
    } else {
        log(Severity::error, "Could not create listener %s: %s", spec.str().c_str(), lp->errorText().c_str());
        return nullptr;
    }
}

void HttpServer::destroyListener(ServerSocket* listener)
{
    for (auto i = listeners_.begin(), e = listeners_.end(); i != e; ++i) {
        if (*i == listener) {
            listeners_.erase(i);
            delete listener;
            break;
        }
    }
}

void HttpServer::setLogLevel(Severity s)
{
    logLevel_ = s;
    logger()->setLevel(s);

    log(s > Severity::info ? s : Severity::info, "Logging level set to: %s", s.c_str());
}

} // namespace x0
