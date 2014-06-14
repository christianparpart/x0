/* <plugins/director/HttpBackend.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "HttpBackend.h"
#include "HttpHealthMonitor.h"
#include "Director.h"

#include <x0/sysconfig.h>
#include <x0/http/HttpServer.h>
#include <x0/io/BufferSource.h>
#include <x0/io/BufferRefSource.h>
#include <x0/io/FileSource.h>
#include <x0/io/SocketSink.h>
#include <x0/CustomDataMgr.h>
#include <x0/SocketSpec.h>
#include <x0/strutils.h>
#include <x0/Utility.h>
#include <x0/Url.h>
#include <x0/Types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#if !defined(XZERO_NDEBUG)
#	define TRACE(msg...) (this->log(LogMessage(Severity::debug1, msg)))
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

// {{{ HttpBackend::Connection API
class HttpBackend::Connection :
#ifndef XZERO_NDEBUG
    public Logging,
#endif
    public CustomData,
    public HttpMessageParser
{
private:
    HttpBackend* backend_;              //!< owning proxy

    RequestNotes* rn_;                  //!< client request
    std::unique_ptr<Socket> socket_;    //!< connection to backend app

    CompositeSource writeSource_;
    SocketSink writeSink_;

    Buffer readBuffer_;
    bool processingDone_;

    char transferPath_[1024];           //!< full path to the temporary file storing the response body
    int transferHandle_;                //!< handle to the response body
    size_t transferOffset_;             //!< number of bytes already passed to the client

    std::string sendfile_;              //!< value of the X-Sendfile backend response header

private:
    HttpBackend* proxy() const { return backend_; }

    void exitSuccess();
    void exitFailure(HttpStatus status);

    bool readSome();
    bool writeSome();
    bool writeSomeBody();

    void onConnected(Socket* s, int revents);
    void onReadWriteReady(Socket* s, int revents);

    void onClientAbort();
    void onWriteComplete();

    void onConnectTimeout(x0::Socket* s);
    void onReadWriteTimeout(x0::Socket* s);

    // response (HttpMessageParser)
    bool onMessageBegin(int versionMajor, int versionMinor, int code, const BufferRef& text) override;
    bool onMessageHeader(const BufferRef& name, const BufferRef& value) override;
    bool onMessageHeaderEnd() override;
    bool onMessageContent(const BufferRef& chunk) override;
    bool onMessageEnd() override;

    void log(x0::LogMessage&& msg) override;

    template<typename... Args>
    void log(Severity severity, const char* fmt, Args&&... args);

    inline void start();
    inline void serializeRequest();

    void inspect(x0::Buffer& out);

public:
    inline explicit Connection(RequestNotes* rn, std::unique_ptr<Socket>&& socket);
    ~Connection();

    static Connection* create(HttpBackend* owner, RequestNotes* rn);
};
// }}}
// {{{ HttpBackend::Connection impl
HttpBackend::Connection::Connection(RequestNotes* rn, std::unique_ptr<Socket>&& socket) :
    HttpMessageParser(HttpMessageParser::RESPONSE),
    backend_(static_cast<HttpBackend*>(rn->backend)),
    rn_(rn),
    socket_(std::move(socket)),

    writeSource_(),
    writeSink_(socket_.get()),

    readBuffer_(),
    processingDone_(false),

    transferPath_(),
    transferHandle_(-1),
    transferOffset_(0)
{
    transferPath_[0] = '\0';

#ifndef XZERO_NDEBUG
    setLoggingPrefix("Connection/%p", this);
#endif
    TRACE("Connection()");

    start();
}

HttpBackend::Connection::~Connection()
{
    // TODO: kill possible pending writeCallback handle (there can be only 0, or 1 if the client aborted early, same for fcgi-backend)
    // XXX alternatively kick off blocking/memaccel modes and support file-cached response only. makes things easier.
    // XXX with the first N bytes kept in memory always, and everything else in file-cache

    if (transferPath_[0] != '\0') {
        unlink(transferPath_);
    }

    if (!(transferHandle_ < 0)) {
        ::close(transferHandle_);
    }
}

void HttpBackend::Connection::exitFailure(HttpStatus status)
{
    TRACE("exitFailure()");
    Backend* backend = backend_;
    RequestNotes* rn = rn_;

    // We failed processing this request, so reschedule
    // this request within the director and give it the chance
    // to be processed by another backend,
    // or give up when the director's request processing
    // timeout has been reached.

    socket_->close();
    rn->request->clearCustomData(backend);
    backend->reject(rn, status);
}

void HttpBackend::Connection::exitSuccess()
{
    TRACE("exitSuccess()");
    Backend* backend = backend_;
    RequestNotes* rn = rn_;

    socket_->close();

    // Notify director that this backend has just completed a request,
    backend->release(rn);

    // We actually served ths request, so finish() it.
    rn->request->finish();
}

void HttpBackend::Connection::onClientAbort()
{
    switch (backend_->manager()->clientAbortAction()) {
        case ClientAbortAction::Ignore:
            log(x0::Severity::diag, "Client closed connection early. Ignored.");
            break;
        case ClientAbortAction::Close:
            log(x0::Severity::diag, "Client closed connection early. Aborting request to backend HTTP server.");
            exitSuccess();
            break;
        case ClientAbortAction::Notify:
            log(x0::Severity::diag, "Client closed connection early. Notifying backend HTTP server by abort.");
            exitSuccess();
            break;
        default:
            // BUG: internal server error
            break;
    }
}

HttpBackend::Connection* HttpBackend::Connection::create(HttpBackend* owner, RequestNotes* rn)
{
    std::unique_ptr<Socket> socket(Socket::open(rn->request->connection.worker().loop(), owner->socketSpec(), O_NONBLOCK | O_CLOEXEC));
    if (!socket)
        return nullptr;

    return rn->request->setCustomData<Connection>(owner, rn, std::move(socket));
}

void HttpBackend::Connection::start()
{
    HttpRequest* r = rn_->request;

    TRACE("Connection.start()");

    r->setAbortHandler(std::bind(&Connection::onClientAbort, this));
    r->registerInspectHandler<HttpBackend::Connection, &HttpBackend::Connection::inspect>(this);

    serializeRequest();

    if (socket_->state() == Socket::Connecting) {
        TRACE("start: connect in progress");
        socket_->setTimeout<Connection, &HttpBackend::Connection::onConnectTimeout>(this, backend_->manager()->connectTimeout());
        socket_->setReadyCallback<Connection, &Connection::onConnected>(this);
    } else { // connected
        TRACE("start: flushing");
        socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(this, backend_->manager()->writeTimeout());
        socket_->setReadyCallback<Connection, &Connection::onReadWriteReady>(this);
        socket_->setMode(Socket::ReadWrite);
    }

    if (backend_->manager()->transferMode() == TransferMode::FileAccel) {
#if defined(O_TMPFILE) && defined(X0_ENABLE_O_TMPFILE)
        static bool otmpfileSupported = true;
        if (otmpfileSupported) {
            transferHandle_ = open(X0_TMPDIR, O_RDWR | O_TMPFILE);
            if (transferHandle_ < 0) {
                // do not attempt to try it again
                otmpfileSupported = false;
            }
        }
#endif

        if (transferHandle_ < 0) {
            snprintf(transferPath_, sizeof(transferPath_), X0_TMPDIR "/x0d-director-%d", socket_->handle());

            transferHandle_ = ::open(transferPath_, O_RDWR | O_CREAT | O_TRUNC, 0666);

            if (transferHandle_ < 0) {
                transferPath_[0] = '\0';
                r->log(Severity::error, "Could not open temporary file %s. %s", transferPath_, strerror(errno));
            }
        }
    }
}

void HttpBackend::Connection::serializeRequest()
{
    HttpRequest* r = rn_->request;
    Buffer writeBuffer(8192);

    // request line
    writeBuffer.push_back(r->method);
    writeBuffer.push_back(' ');
    writeBuffer.push_back(r->unparsedUri);
    writeBuffer.push_back(" HTTP/1.1\r\n");

    BufferRef forwardedFor;

    // request headers
    for (auto& header: r->requestHeaders) {
        if (iequals(header.name, "X-Forwarded-For")) {
            forwardedFor = header.value;
            continue;
        }
        else if (iequals(header.name, "Content-Transfer")
                || iequals(header.name, "Expect")
                || iequals(header.name, "Connection")) {
            TRACE("skip requestHeader(%s: %s)", header.name.str().c_str(), header.value.str().c_str());
            continue;
        }

        TRACE("pass requestHeader(%s: %s)", header.name.str().c_str(), header.value.str().c_str());
        writeBuffer.push_back(header.name);
        writeBuffer.push_back(": ");
        writeBuffer.push_back(header.value);
        writeBuffer.push_back("\r\n");
    }

    // additional headers to add
    writeBuffer.push_back("Connection: closed\r\n");

    // X-Forwarded-For
    writeBuffer.push_back("X-Forwarded-For: ");
    if (forwardedFor) {
        writeBuffer.push_back(forwardedFor);
        writeBuffer.push_back(", ");
    }
    writeBuffer.push_back(r->connection.remoteIP().str());
    writeBuffer.push_back("\r\n");

    // X-Forwarded-Proto
    if (r->requestHeader("X-Forwarded-Proto").empty()) {
        if (r->connection.isSecure())
            writeBuffer.push_back("X-Forwarded-Proto: https\r\n");
        else
            writeBuffer.push_back("X-Forwarded-Proto: http\r\n");
    }

    // request headers terminator
    writeBuffer.push_back("\r\n");

    writeSource_.push_back<BufferSource>(std::move(writeBuffer));

    if (r->contentAvailable()) {
        writeSource_.push_back(std::move(r->takeBody()));
    }
}

/**
 * connect() timeout callback.
 *
 * This callback is invoked from within the requests associated thread to notify about
 * a timed out read/write operation.
 */
void HttpBackend::Connection::onConnectTimeout(x0::Socket* s)
{
    rn_->request->log(Severity::error, "http-proxy: Failed to connect to backend %s. Timed out.", backend_->name().c_str());

    backend_->setState(HealthState::Offline);
    exitFailure(HttpStatus::GatewayTimeout);
}

/**
 * read()/write() timeout callback.
 *
 * This callback is invoked from within the requests associated thread to notify about
 * a timed out read/write operation.
 */
void HttpBackend::Connection::onReadWriteTimeout(x0::Socket* s)
{
    rn_->request->log(x0::Severity::error, "http-proxy: Failed to perform I/O on backend %s. Timed out", backend_->name().c_str());
    backend_->setState(HealthState::Offline);

    exitFailure(HttpStatus::GatewayTimeout);
}

void HttpBackend::Connection::onConnected(Socket* s, int revents)
{
    TRACE("onConnected");

    if (socket_->state() == Socket::Operational) {
        TRACE("onConnected: flushing");
        socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(this, backend_->manager()->writeTimeout());
        socket_->setReadyCallback<Connection, &Connection::onReadWriteReady>(this);
        socket_->setMode(Socket::ReadWrite); // flush already serialized request
    } else {
        TRACE("onConnected: failed");
        rn_->request->log(Severity::error, "HTTP proxy: Could not connect to backend: %s", strerror(errno));
        backend_->setState(HealthState::Offline);
        exitFailure(HttpStatus::ServiceUnavailable);
    }
}

/** callback, invoked when the origin server has passed us the response status line.
 *
 * We will use the status code only.
 * However, we could pass the text field, too - once x0 core supports it.
 */
bool HttpBackend::Connection::onMessageBegin(int major, int minor, int code, const BufferRef& text)
{
    TRACE("Connection(%p).status(HTTP/%d.%d, %d, '%s')", (void*)this, major, minor, code, text.str().c_str());

    rn_->request->status = static_cast<HttpStatus>(code);
    TRACE("status: %d", (int)rn_->request->status);
    return true;
}

/** callback, invoked on every successfully parsed response header.
 *
 * We will pass this header directly to the client's response,
 * if that is NOT a connection-level header.
 */
bool HttpBackend::Connection::onMessageHeader(const BufferRef& name, const BufferRef& value)
{
    TRACE("Connection(%p).onHeader('%s', '%s')", (void*)this, name.str().c_str(), value.str().c_str());

    // XXX do not allow origin's connection-level response headers to be passed to the client.
    if (iequals(name, "Connection"))
        goto skip;

    if (iequals(name, "Transfer-Encoding"))
        goto skip;

    if (unlikely(iequals(name, "X-Sendfile"))) {
        sendfile_ = value.str();
        goto skip;
    }

    rn_->request->responseHeaders.push_back(name.str(), value.str());
    return true;

skip:
    TRACE("skip (connection-)level header");

    return true;
}

bool HttpBackend::Connection::onMessageHeaderEnd()
{
    TRACE("onMessageHeaderEnd()");

    if (rn_->request->method == "HEAD") {
        processingDone_ = true;
    }

    if (unlikely(!sendfile_.empty())) {
        auto r = rn_->request;
        r->responseHeaders.remove("Content-Type");
        r->responseHeaders.remove("Content-Length");
        r->responseHeaders.remove("ETag");
        r->sendfile(sendfile_);
    }

    return true;
}

/** callback, invoked on a new response content chunk. */
bool HttpBackend::Connection::onMessageContent(const BufferRef& chunk)
{
    TRACE("messageContent(nb:%lu) state:%s", chunk.size(), socket_->state_str());

    if (unlikely(!sendfile_.empty()))
        // we ignore the backend's message body as we've replaced it with the file contents of X-Sendfile's file.
        return true;

    switch (backend_->manager()->transferMode()) {
    case TransferMode::FileAccel:
        if (!(transferHandle_ < 0)) {
            ssize_t rv = ::write(transferHandle_, chunk.data(), chunk.size());
            if (rv == static_cast<ssize_t>(chunk.size())) {
                rn_->request->write<FileSource>(transferHandle_, transferOffset_, rv, false);
                transferOffset_ += rv;
                break;
            } else if (rv > 0) {
                // partial write to disk (is this possible?) -- TODO: investigate if that case is possible
                transferOffset_ += rv;
            }
        }
        // fall through
    case TransferMode::MemoryAccel:
        rn_->request->write<BufferRefSource>(chunk);
        break;
    case TransferMode::Blocking:
        // stop watching for more input
        socket_->setMode(Socket::None);

        // transfer response-body chunk to client
        rn_->request->write<BufferRefSource>(chunk);

        // start listening on backend I/O when chunk has been fully transmitted
        rn_->request->writeCallback<Connection, &Connection::onWriteComplete>(this);
        break;
    }

    return true;
}

void HttpBackend::Connection::onWriteComplete()
{
    if (!socket_->isOpen())
        return;

    TRACE("chunk write complete: %s", state_str());
    socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(this, backend_->manager()->readTimeout());
    socket_->setMode(Socket::Read);
}

bool HttpBackend::Connection::onMessageEnd()
{
    TRACE("messageEnd() backend-state:%s", socket_->state_str());
    processingDone_ = true;
    return false;
}

void HttpBackend::Connection::log(x0::LogMessage&& msg)
{
    if (rn_) {
        msg.addTag("http-backend");
        rn_->request->log(std::move(msg));
    }
}

template<typename... Args>
inline void HttpBackend::Connection::log(Severity severity, const char* fmt, Args&&... args)
{
    log(LogMessage(severity, fmt, args...));
}

void HttpBackend::Connection::onReadWriteReady(Socket* s, int revents)
{
    TRACE("io(0x%04x)", revents);

    if (revents & Socket::Read) {
        if (!readSome()) {
            return;
        }
    }

    if (revents & Socket::Write) {
        if (!writeSome()) {
            return;
        }
    }
}

bool HttpBackend::Connection::writeSome()
{
    auto r = rn_->request;
    TRACE("writeSome() - %s", state_str());

    ssize_t rv = writeSource_.sendto(writeSink_);
    TRACE("write request: wrote %ld bytes", rv);

    if (rv == 0) {
        socket_->setMode(Socket::Read);
    }
    else if (rv > 0) {
        socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(this, backend_->manager()->writeTimeout());
    } else if (rv < 0) {
        switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        case EWOULDBLOCK:
#endif
        case EAGAIN:
        case EINTR:
            socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(this, backend_->manager()->writeTimeout());
            socket_->setMode(Socket::ReadWrite);
            break;
        default:
            r->log(Severity::error, "Writing to backend %s failed. %s", backend_->socketSpec().str().c_str(), strerror(errno));
            backend_->setState(HealthState::Offline);
            exitFailure(HttpStatus::ServiceUnavailable);
            return false;
        }
    }
    return true;
}

bool HttpBackend::Connection::readSome()
{
    TRACE("readSome() - %s", state_str());

    std::size_t lower_bound = readBuffer_.size();

    if (lower_bound == readBuffer_.capacity())
        readBuffer_.setCapacity(lower_bound + 4096);

    ssize_t rv = socket_->read(readBuffer_);

    if (rv > 0) {
        TRACE("read response: %ld bytes", rv);
        std::size_t np = parseFragment(readBuffer_.ref(lower_bound, rv));
        (void) np;
        TRACE("readSome(): parseFragment(): %ld / %ld", np, rv);

        if (processingDone_) {
            exitSuccess();
            return false;
        } else if (state() == PROTOCOL_ERROR) {
            rn_->request->log(Severity::error, "Reading response from backend %s failed. Protocol Error.", backend_->socketSpec().str().c_str());
            backend_->setState(HealthState::Offline);
            exitFailure(HttpStatus::ServiceUnavailable);
            return false;
        } else {
            TRACE("resume with io:%d, state:%s", socket_->mode(), socket_->state_str());
            socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(this, backend_->manager()->readTimeout());
            socket_->setMode(Socket::Read);
        }
    } else if (rv == 0) {
        TRACE("http server connection closed");
        if (!processingDone_) {
            if (!rn_->request->status)
                exitFailure(HttpStatus::ServiceUnavailable);
            else
                exitSuccess();
        }
        return false;
    } else {
        switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        case EWOULDBLOCK:
#endif
        case EAGAIN:
        case EINTR:
            socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(this, backend_->manager()->readTimeout());
            socket_->setMode(Socket::Read);
            break;
        default:
            rn_->request->log(Severity::error, "Reading response from backend %s failed. Syntax Error.", backend_->socketSpec().str().c_str());
            backend_->setState(HealthState::Offline);
            exitFailure(HttpStatus::ServiceUnavailable);
            return false;
        }
    }
    return true;
}

void HttpBackend::Connection::inspect(x0::Buffer& out)
{
    out << "processingDone:" << (processingDone_ ? "yes" : "no") << "\n";

    if (socket_) {
        out << "backend-socket: ";
        socket_->inspect(out);
    }

	if (rn_) {
        rn_->inspect(out);
		out << "client.isOutputPending:" << rn_->request->connection.isOutputPending() << '\n';
	} else {
		out << "no-client-request-bound!\n";
	}
}
// }}}
// {{{ HttpBackend impl
HttpBackend::HttpBackend(BackendManager* bm, const std::string& name,
        const SocketSpec& socketSpec, size_t capacity, bool healthChecks) :
    Backend(bm, name, socketSpec, capacity, healthChecks ? std::make_unique<HttpHealthMonitor>(*bm->worker()->server().nextWorker()) : nullptr)
{
#ifndef XZERO_NDEBUG
    setLoggingPrefix("HttpBackend/%s", name.c_str());
#endif

    if (healthChecks) {
        healthMonitor()->setBackend(this);
    }
}

HttpBackend::~HttpBackend()
{
}

const std::string& HttpBackend::protocol() const
{
    static const std::string value("http");
    return value;
}

bool HttpBackend::process(RequestNotes* rn)
{
    if (Connection::create(this, rn))
        return true;

    rn->request->log(Severity::error, "HTTP proxy: Could not connect to backend %s. %s", socketSpec_.str().c_str(), strerror(errno));

    return false;
}
// }}}
