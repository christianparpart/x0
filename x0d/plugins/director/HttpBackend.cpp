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
    public HttpMessageParser
{
private:
    HttpBackend* backend_;			//!< owning proxy

    int refCount_;

    RequestNotes* rn_;			//!< client request
    Socket* socket_;			//!< connection to backend app

    Buffer writeBuffer_;
    size_t writeOffset_;
    size_t writeProgress_;

    Buffer readBuffer_;
    bool processingDone_;

    int transferHandle_;
    size_t transferOffset_;

    std::string sendfile_;

private:
    HttpBackend* proxy() const { return backend_; }

    void ref();
    void unref();

    inline void close();
    inline void readSome();
    inline void writeSome();

    void onConnected(Socket* s, int revents);
    void io(Socket* s, int revents);
    void onRequestChunk(const BufferRef& chunk);

    static void onClientAbort(void *p);
    void onWriteComplete();

    void onConnectTimeout(x0::Socket* s);
    void onTimeout(x0::Socket* s);

    // response (HttpMessageParser)
    virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const BufferRef& text);
    virtual bool onMessageHeader(const BufferRef& name, const BufferRef& value);
    virtual bool onMessageHeaderEnd();
    virtual bool onMessageContent(const BufferRef& chunk);
    virtual bool onMessageEnd();

    virtual void log(x0::LogMessage&& msg);

    template<typename... Args>
    void log(Severity severity, const char* fmt, Args&&... args);

    inline void start();

public:
    inline explicit Connection(HttpBackend* proxy, RequestNotes* rn, Socket* socket);
    ~Connection();

    static Connection* create(HttpBackend* owner, RequestNotes* rn);
};
// }}}
// {{{ HttpBackend::Connection impl
HttpBackend::Connection::Connection(HttpBackend* proxy, RequestNotes* rn, Socket* socket) :
    HttpMessageParser(HttpMessageParser::RESPONSE),
    backend_(proxy),
    refCount_(1),
    rn_(rn),
    socket_(socket),

    writeBuffer_(),
    writeOffset_(0),
    writeProgress_(0),
    readBuffer_(),
    processingDone_(false),

    transferHandle_(-1),
    transferOffset_(0)
{
#ifndef XZERO_NDEBUG
    setLoggingPrefix("Connection/%p", this);
#endif
    TRACE("Connection()");

    start();
}

HttpBackend::Connection::~Connection()
{
    TRACE("~Connection()");

    if (socket_) {
        delete socket_;
    }

    if (!(transferHandle_ < 0)) {
        ::close(transferHandle_);
    }

    if (rn_) {
        if (rn_->request->status == HttpStatus::Undefined && !rn_->request->isAborted()) {
            // We failed processing this request, so reschedule
            // this request within the director and give it the chance
            // to be processed by another backend,
            // or give up when the director's request processing
            // timeout has been reached.

            rn_->request->log(Severity::notice, "Reading response from backend %s failed. Backend closed connection early.", backend_->socketSpec().str().c_str());
            backend_->reject(rn_);
        } else {
            // Notify director that this backend has just completed a request,
            backend_->release(rn_);

            // We actually served ths request, so finish() it.
            rn_->request->finish();
        }
    }
}

void HttpBackend::Connection::ref()
{
    ++refCount_;
}

void HttpBackend::Connection::unref()
{
    assert(refCount_ > 0);

    --refCount_;

    if (refCount_ == 0) {
        delete this;
    }
}

void HttpBackend::Connection::close()
{
    if (socket_->isOpen()) {
        // stop watching on any backend I/O events, if active
        socket_->close();

        // unref the ctor's ref
        unref();
    }
}

void HttpBackend::Connection::onClientAbort(void *p)
{
    Connection *self = reinterpret_cast<Connection *>(p);
    self->log(x0::Severity::diag, "Client closed connection early. Aborting request to upstream HTTP server.");
    self->close();
}

HttpBackend::Connection* HttpBackend::Connection::create(HttpBackend* owner, RequestNotes* rn)
{
    Socket* socket = Socket::open(rn->request->connection.worker().loop(), owner->socketSpec(), O_NONBLOCK | O_CLOEXEC);
    if (!socket)
        return nullptr;

    return new Connection(owner, rn, socket);
}

void HttpBackend::Connection::start()
{
    auto r = rn_->request;

    TRACE("Connection.start()");

    r->setAbortHandler(&Connection::onClientAbort, this);

    // request line
    writeBuffer_.push_back(r->method);
    writeBuffer_.push_back(' ');
    writeBuffer_.push_back(r->unparsedUri);
    writeBuffer_.push_back(" HTTP/1.1\r\n");

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
        writeBuffer_.push_back(header.name);
        writeBuffer_.push_back(": ");
        writeBuffer_.push_back(header.value);
        writeBuffer_.push_back("\r\n");
    }

    // additional headers to add
    writeBuffer_.push_back("Connection: closed\r\n");

    // X-Forwarded-For
    writeBuffer_.push_back("X-Forwarded-For: ");
    if (forwardedFor) {
        writeBuffer_.push_back(forwardedFor);
        writeBuffer_.push_back(", ");
    }
    writeBuffer_.push_back(r->connection.remoteIP().str());
    writeBuffer_.push_back("\r\n");

    // X-Forwarded-Proto
    if (r->requestHeader("X-Forwarded-Proto").empty()) {
        if (r->connection.isSecure())
            writeBuffer_.push_back("X-Forwarded-Proto: https\r\n");
        else
            writeBuffer_.push_back("X-Forwarded-Proto: http\r\n");
    }

    // request headers terminator
    writeBuffer_.push_back("\r\n");

    if (r->contentAvailable()) {
        TRACE("start: request content available: reading.");
        r->setBodyCallback<Connection, &Connection::onRequestChunk>(this);
    }

    if (socket_->state() == Socket::Connecting) {
        TRACE("start: connect in progress");
        socket_->setTimeout<Connection, &HttpBackend::Connection::onConnectTimeout>(this, backend_->manager()->connectTimeout());
        socket_->setReadyCallback<Connection, &Connection::onConnected>(this);
    } else { // connected
        TRACE("start: flushing");
        socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->writeTimeout());
        socket_->setReadyCallback<Connection, &Connection::io>(this);
        socket_->setMode(Socket::ReadWrite);
    }

    if (backend_->manager()->transferMode() == TransferMode::FileAccel) {
        char path[1024];
        snprintf(path, sizeof(path), "/tmp/x0d-director-%d", socket_->handle());

        transferHandle_ = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (transferHandle_ < 0) {
            r->log(Severity::error, "Could not open temporary file %s. %s", path, strerror(errno));
        }
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

    if (!rn_->request->status)
        rn_->request->status = HttpStatus::GatewayTimeout;

    backend_->setState(HealthState::Offline);
    close();
}

/**
 * read()/write() timeout callback.
 *
 * This callback is invoked from within the requests associated thread to notify about
 * a timed out read/write operation.
 */
void HttpBackend::Connection::onTimeout(x0::Socket* s)
{
    rn_->request->log(x0::Severity::error, "http-proxy: Failed to perform I/O on backend %s. Timed out", backend_->name().c_str());
    backend_->setState(HealthState::Offline);

    if (!rn_->request->status)
        rn_->request->status = HttpStatus::GatewayTimeout;

    close();
}

void HttpBackend::Connection::onConnected(Socket* s, int revents)
{
    TRACE("onConnected: content? %d", rn_->request->contentAvailable());
    //TRACE("onConnected.pending:\n%s\n", writeBuffer_.c_str());

    if (socket_->state() == Socket::Operational) {
        TRACE("onConnected: flushing");
        socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->writeTimeout());
        socket_->setReadyCallback<Connection, &Connection::io>(this);
        socket_->setMode(Socket::ReadWrite); // flush already serialized request
    } else {
        TRACE("onConnected: failed");
        rn_->request->log(Severity::error, "HTTP proxy: Could not connect to backend: %s", strerror(errno));
        backend_->setState(HealthState::Offline);

        // XXX explicit unref instead of close() here, because close() doesn't cover unref(), as it's only unref'ing when socket is open.
        unref();
    }
}

/** transferres a request body chunk to the origin server.  */
void HttpBackend::Connection::onRequestChunk(const BufferRef& chunk)
{
    TRACE("onRequestChunk(nb:%ld)", chunk.size());
    writeBuffer_.push_back(chunk);

    if (socket_->state() == Socket::Operational) {
        socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->writeTimeout());
        socket_->setMode(Socket::ReadWrite);
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
        ref();
        rn_->request->writeCallback<Connection, &Connection::onWriteComplete>(this);
        break;
    }

    return true;
}

void HttpBackend::Connection::onWriteComplete()
{
    TRACE("chunk write complete: %s", state_str());
    socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->readTimeout());
    socket_->setMode(Socket::Read);
    unref();
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

void HttpBackend::Connection::io(Socket* s, int revents)
{
    TRACE("io(0x%04x)", revents);

    if (revents & Socket::Read)
        readSome();

    if (revents & Socket::Write)
        writeSome();
}

void HttpBackend::Connection::writeSome()
{
    auto r = rn_->request;
    TRACE("writeSome() - %s (%d)", state_str(), r->contentAvailable());

    ssize_t rv = socket_->write(writeBuffer_.data() + writeOffset_, writeBuffer_.size() - writeOffset_);

    if (rv > 0) {
        TRACE("write request: %ld (of %ld) bytes", rv, writeBuffer_.size() - writeOffset_);

        writeOffset_ += rv;
        writeProgress_ += rv;

        if (writeOffset_ == writeBuffer_.size()) {
            TRACE("writeOffset == writeBuffser.size (%ld) p:%ld, ca: %d, clr:%ld", writeOffset_,
                writeProgress_, r->contentAvailable(), r->connection.contentLength());

            writeOffset_ = 0;
            writeBuffer_.clear();
            socket_->setMode(Socket::Read);
        } else {
            socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->writeTimeout());
        }
    } else if (rv < 0) {
        switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        case EWOULDBLOCK:
#endif
        case EAGAIN:
        case EINTR:
            socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->writeTimeout());
            socket_->setMode(Socket::ReadWrite);
            break;
        default:
            r->log(Severity::error, "Writing to backend %s failed. %s", backend_->socketSpec().str().c_str(), strerror(errno));
            backend_->setState(HealthState::Offline);
            close();
            break;
        }
    }
}

void HttpBackend::Connection::readSome()
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
            close();
        } else if (state() == PROTOCOL_ERROR) {
            rn_->request->log(Severity::error, "Reading response from backend %s failed. Protocol Error.", backend_->socketSpec().str().c_str());
            backend_->setState(HealthState::Offline);
            close();
        } else {
            TRACE("resume with io:%d, state:%s", socket_->mode(), socket_->state_str());
            socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->readTimeout());
            socket_->setMode(Socket::Read);
        }
    } else if (rv == 0) {
        TRACE("http server connection closed");
        close();
    } else {
        switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        case EWOULDBLOCK:
#endif
        case EAGAIN:
        case EINTR:
            socket_->setTimeout<Connection, &Connection::onTimeout>(this, backend_->manager()->readTimeout());
            socket_->setMode(Socket::Read);
            break;
        default:
            rn_->request->log(Severity::error, "Reading response from backend %s failed. Syntax Error.", backend_->socketSpec().str().c_str());
            backend_->setState(HealthState::Offline);
            close();
            break;
        }
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
