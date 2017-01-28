// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "HttpBackend.h"
#include "HttpHealthMonitor.h"
#include "Director.h"

#include <x0d/sysconfig.h>
#include <xzero/HttpServer.h>
#include <base/io/BufferSource.h>
#include <base/io/BufferRefSource.h>
#include <base/io/FileSource.h>
#include <base/io/SocketSink.h>
#include <base/CustomDataMgr.h>
#include <base/SocketSpec.h>
#include <base/strutils.h>
#include <base/Utility.h>
#include <base/Url.h>
#include <base/Types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#if !defined(NDEBUG)
#define TRACE(msg...) (this->log(Severity::trace1, msg))
#else
#define TRACE(msg...) \
  do {                \
  } while (0)
#endif

using namespace base;
using namespace xzero;

// {{{ HttpBackend::Connection API
class HttpBackend::Connection :
#ifndef NDEBUG
    public Logging,
#endif
    public CustomData,
    public HttpMessageParser {
 private:
  HttpBackend* backend_;  //!< owning proxy

  RequestNotes* rn_;                //!< client request
  std::unique_ptr<Socket> socket_;  //!< connection to backend app

  CompositeSource writeSource_;
  SocketSink writeSink_;

  Buffer readBuffer_;
  bool processingDone_;

  char transferPath_[1024];  //!< full path to the temporary file storing the
                             //response body
  int transferHandle_;       //!< handle to the response body
  size_t transferOffset_;    //!< number of bytes already passed to the client

  std::string sendfile_;  //!< value of the X-Sendfile backend response header

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

  void onConnectTimeout(Socket* s);
  void onReadWriteTimeout(Socket* s);

  // response (HttpMessageParser)
  bool onMessageBegin(int versionMajor, int versionMinor, int code,
                      const BufferRef& text) override;
  bool onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  bool onMessageHeaderEnd() override;
  bool onMessageContent(const BufferRef& chunk) override;
  bool onMessageEnd() override;

  template <typename... Args>
  void log(Severity severity, const char* fmt, Args&&... args);

  inline void start();
  inline void serializeRequest();

  void inspect(Buffer& out);

 public:
  inline explicit Connection(RequestNotes* rn,
                             std::unique_ptr<Socket>&& socket);
  ~Connection();

  static Connection* create(HttpBackend* owner, RequestNotes* rn);
};
// }}}
// {{{ HttpBackend::Connection impl
HttpBackend::Connection::Connection(RequestNotes* rn,
                                    std::unique_ptr<Socket>&& socket)
    : HttpMessageParser(HttpMessageParser::RESPONSE),
      backend_(static_cast<HttpBackend*>(rn->backend)),
      rn_(rn),
      socket_(std::move(socket)),
      writeSource_(),
      writeSink_(socket_.get()),
      readBuffer_(),
      processingDone_(false),
      transferPath_(),
      transferHandle_(-1),
      transferOffset_(0) {
  transferPath_[0] = '\0';

#ifndef NDEBUG
  setLoggingPrefix("Connection/%p", this);
#endif
  TRACE("Connection()");

  start();
}

HttpBackend::Connection::~Connection() {
  // TODO: kill possible pending writeCallback handle (there can be only 0, or 1
  // if the client aborted early, same for fcgi-backend)
  // XXX alternatively kick off blocking/memaccel modes and support file-cached
  // response only. makes things easier.
  // XXX with the first N bytes kept in memory always, and everything else in
  // file-cache

  if (transferPath_[0] != '\0') {
    unlink(transferPath_);
  }

  if (!(transferHandle_ < 0)) {
    ::close(transferHandle_);
  }
}

void HttpBackend::Connection::exitFailure(HttpStatus status) {
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

void HttpBackend::Connection::exitSuccess() {
  TRACE("exitSuccess()");
  Backend* backend = backend_;
  RequestNotes* rn = rn_;

  socket_->close();

  // Notify director that this backend has just completed a request,
  backend->release(rn);

  // We actually served ths request, so finish() it.
  rn->request->finish();
}

void HttpBackend::Connection::onClientAbort() {
  switch (backend_->manager()->clientAbortAction()) {
    case ClientAbortAction::Ignore:
      log(Severity::debug, "Client closed connection early. Ignored.");
      break;
    case ClientAbortAction::Close:
      log(Severity::debug,
          "Client closed connection early. Aborting request to backend HTTP "
          "server.");
      exitSuccess();
      break;
    case ClientAbortAction::Notify:
      log(Severity::debug,
          "Client closed connection early. Notifying backend HTTP server by "
          "abort.");
      exitSuccess();
      break;
    default:
      // BUG: internal server error
      break;
  }
}

HttpBackend::Connection* HttpBackend::Connection::create(HttpBackend* owner,
                                                         RequestNotes* rn) {
  std::unique_ptr<Socket> socket(
      Socket::open(rn->request->connection.worker().loop(), owner->socketSpec(),
                   O_NONBLOCK | O_CLOEXEC));
  if (!socket) return nullptr;

  return rn->request->setCustomData<Connection>(owner, rn, std::move(socket));
}

void HttpBackend::Connection::start() {
  HttpRequest* r = rn_->request;

  TRACE("Connection.start()");

  r->setAbortHandler(std::bind(&Connection::onClientAbort, this));
  r->registerInspectHandler<HttpBackend::Connection,
                            &HttpBackend::Connection::inspect>(this);

  serializeRequest();

  if (socket_->state() == Socket::Connecting) {
    TRACE("start: connect in progress");
    socket_->setTimeout<Connection, &HttpBackend::Connection::onConnectTimeout>(
        this, backend_->manager()->connectTimeout());
    socket_->setReadyCallback<Connection, &Connection::onConnected>(this);
  } else {  // connected
    TRACE("start: flushing");
    socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_->setReadyCallback<Connection, &Connection::onReadWriteReady>(this);
    socket_->setMode(Socket::ReadWrite);
  }

  if (true) {
#if defined(O_TMPFILE) && defined(ENABLE_O_TMPFILE)
    static bool otmpfileSupported = true;
    if (otmpfileSupported) {
      const int flags = O_RDWR | O_TMPFILE;
      transferHandle_ = open(HttpConnection::tempDirectory().c_str(), flags);
      if (transferHandle_ < 0) {
        // do not attempt to try it again
        otmpfileSupported = false;
      }
    }
#endif

    if (transferHandle_ < 0) {
      snprintf(transferPath_, sizeof(transferPath_), "%s/x0d-director-%d",
               HttpConnection::tempDirectory().c_str(), socket_->handle());

      transferHandle_ = ::open(transferPath_, O_RDWR | O_CREAT | O_TRUNC, 0666);

      if (transferHandle_ < 0) {
        r->log(Severity::error, "Could not open temporary file %s. %s",
               transferPath_, strerror(errno));
        transferPath_[0] = '\0';
      }
    }
  }
}

void HttpBackend::Connection::serializeRequest() {
  HttpRequest* r = rn_->request;
  Buffer writeBuffer(8192);

  // request line
  writeBuffer.push_back(r->method);
  writeBuffer.push_back(' ');
  writeBuffer.push_back(r->unparsedUri);
  writeBuffer.push_back(" HTTP/1.1\r\n");

  BufferRef forwardedFor;

  // request headers
  for (auto& header : r->requestHeaders) {
    if (iequals(header.name, "X-Forwarded-For")) {
      forwardedFor = header.value;
      continue;
    } else if (iequals(header.name, "Content-Transfer") ||
               iequals(header.name, "Expect") ||
               iequals(header.name, "Connection")) {
      TRACE("skip requestHeader($0: $1)", header.name, header.value);
      continue;
    }

    TRACE("pass requestHeader($0: $1)", header.name, header.value);
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
 * This callback is invoked from within the requests associated thread to notify
 *about
 * a timed out read/write operation.
 */
void HttpBackend::Connection::onConnectTimeout(Socket* s) {
  rn_->request->log(Severity::error,
                    "http-proxy: Failed to connect to backend %s. Timed out.",
                    backend_->name().c_str());

  backend_->setState(HealthState::Offline);
  exitFailure(HttpStatus::GatewayTimeout);
}

/**
 * read()/write() timeout callback.
 *
 * This callback is invoked from within the requests associated thread to notify
 *about
 * a timed out read/write operation.
 */
void HttpBackend::Connection::onReadWriteTimeout(Socket* s) {
  rn_->request->log(
      Severity::error,
      "http-proxy: Failed to perform I/O on backend %s. Timed out",
      backend_->name().c_str());
  backend_->setState(HealthState::Offline);

  exitFailure(HttpStatus::GatewayTimeout);
}

void HttpBackend::Connection::onConnected(Socket* s, int revents) {
  TRACE("onConnected");

  if (socket_->state() == Socket::Operational) {
    TRACE("onConnected: flushing");
    socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_->setReadyCallback<Connection, &Connection::onReadWriteReady>(this);
    socket_->setMode(Socket::ReadWrite);  // flush already serialized request
  } else {
    TRACE("onConnected: failed");
    rn_->request->log(Severity::error,
                      "HTTP proxy: Could not connect to backend: %s",
                      strerror(errno));
    backend_->setState(HealthState::Offline);
    exitFailure(HttpStatus::ServiceUnavailable);
  }
}

/** callback, invoked when the origin server has passed us the response status
 *line.
 *
 * We will use the status code only.
 * However, we could pass the text field, too - once x0 core supports it.
 */
bool HttpBackend::Connection::onMessageBegin(int major, int minor, int code,
                                             const BufferRef& text) {
  TRACE("Connection($0).status(HTTP/$0.$1, '$2')",
        (void*)this, major, minor, code, text);

  rn_->request->status = static_cast<HttpStatus>(code);
  TRACE("status: $0", rn_->request->status);
  return true;
}

/** callback, invoked on every successfully parsed response header.
 *
 * We will pass this header directly to the client's response,
 * if that is NOT a connection-level header.
 */
bool HttpBackend::Connection::onMessageHeader(const BufferRef& name,
                                              const BufferRef& value) {
  TRACE("Connection($0).onHeader('$1', '$2')", (void*)this, name, value);

  // XXX do not allow origin's connection-level response headers to be passed to
  // the client.
  if (iequals(name, "Connection")) goto skip;

  if (iequals(name, "Transfer-Encoding")) goto skip;

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

bool HttpBackend::Connection::onMessageHeaderEnd() {
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
bool HttpBackend::Connection::onMessageContent(const BufferRef& chunk) {
  TRACE("messageContent(nb:$0) state:$1", chunk.size(), socket_->state_str());

  if (unlikely(!sendfile_.empty()))
    // we ignore the backend's message body as we've replaced it with the file
    // contents of X-Sendfile's file.
    return true;

  if (!(transferHandle_ < 0)) {
    ssize_t rv = ::write(transferHandle_, chunk.data(), chunk.size());
    if (rv == static_cast<ssize_t>(chunk.size())) {
      rn_->request->write<FileSource>(transferHandle_, transferOffset_, rv,
                                      false);
      transferOffset_ += rv;
      return true;
    } else if (rv > 0) {
      // partial write to disk (is this possible?) -- TODO: investigate if that
      // case is possible
      transferOffset_ += rv;
    }
  }

  rn_->request->write<BufferRefSource>(chunk);
  return true;
}

bool HttpBackend::Connection::onMessageEnd() {
  TRACE("messageEnd() backend-state:$0", socket_->state_str());
  processingDone_ = true;
  return false;
}

template <typename... Args>
inline void HttpBackend::Connection::log(Severity severity, const char* fmt,
                                         Args&&... args) {
  if (rn_) {
    rn_->request->log(severity, fmt, std::forward<Args>(args)...);
  }
}

void HttpBackend::Connection::onReadWriteReady(Socket* s, int revents) {
  TRACE("io($0)", revents);

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

bool HttpBackend::Connection::writeSome() {
  auto r = rn_->request;
  TRACE("writeSome() - $0", state());

  ssize_t rv = writeSource_.sendto(writeSink_);
  TRACE("write request: wrote $0 bytes", rv);

  if (rv == 0) {
    // output fully flushed. continue to read response
    socket_->setMode(Socket::Read);
  } else if (rv > 0) {
    // we wrote something
    socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(
        this, backend_->manager()->writeTimeout());
  } else if (rv < 0) {
    // upstream write error
    switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
      case EWOULDBLOCK:
#endif
      case EAGAIN:
      case EINTR:
        socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(
            this, backend_->manager()->writeTimeout());
        socket_->setMode(Socket::ReadWrite);
        break;
      default:
        r->log(Severity::error, "Writing to backend %s failed. %s",
               backend_->socketSpec().str().c_str(), strerror(errno));
        backend_->setState(HealthState::Offline);
        exitFailure(HttpStatus::ServiceUnavailable);
        return false;
    }
  }
  return true;
}

bool HttpBackend::Connection::readSome() {
  TRACE("readSome() - $0", state());

  std::size_t lower_bound = readBuffer_.size();

  if (lower_bound == readBuffer_.capacity())
    readBuffer_.setCapacity(lower_bound + 4096);

  ssize_t rv = socket_->read(readBuffer_);

  if (rv > 0) {
    TRACE("read response: $0 bytes", rv);
    std::size_t np = parseFragment(readBuffer_.ref(lower_bound, rv));
    (void)np;
    TRACE("readSome(): parseFragment(): $0 / $1", np, rv);

    if (processingDone_) {
      exitSuccess();
      return false;
    } else if (state() == PROTOCOL_ERROR) {
      rn_->request->log(
          Severity::error,
          "Reading response from backend %s failed. Protocol Error.",
          backend_->socketSpec().str().c_str());
      backend_->setState(HealthState::Offline);
      exitFailure(HttpStatus::ServiceUnavailable);
      return false;
    } else {
      TRACE("resume with io:$0, state:$1", socket_->mode(), socket_->state());
      socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(
          this, backend_->manager()->readTimeout());
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
        socket_->setTimeout<Connection, &Connection::onReadWriteTimeout>(
            this, backend_->manager()->readTimeout());
        socket_->setMode(Socket::Read);
        break;
      default:
        rn_->request->log(
            Severity::error,
            "Reading response from backend %s failed. Syntax Error.",
            backend_->socketSpec().str().c_str());
        backend_->setState(HealthState::Offline);
        exitFailure(HttpStatus::ServiceUnavailable);
        return false;
    }
  }
  return true;
}

void HttpBackend::Connection::inspect(Buffer& out) {
  out << "processingDone:" << (processingDone_ ? "yes" : "no") << "\n";

  if (socket_) {
    out << "backend-socket: ";
    socket_->inspect(out);
  }

  if (rn_) {
    rn_->inspect(out);
    out << "client.isOutputPending:"
        << rn_->request->connection.isOutputPending() << '\n';
  } else {
    out << "no-client-request-bound!\n";
  }
}
// }}}
// {{{ HttpBackend impl
HttpBackend::HttpBackend(BackendManager* bm, const std::string& name,
                         const SocketSpec& socketSpec, size_t capacity,
                         bool healthChecks)
    : Backend(bm, name, socketSpec, capacity,
              healthChecks ? std::make_unique<HttpHealthMonitor>(
                                 *bm->worker()->server().nextWorker())
                           : nullptr) {
#ifndef NDEBUG
  setLoggingPrefix("HttpBackend/%s", name.c_str());
#endif

  if (healthChecks) {
    healthMonitor()->setBackend(this);
  }
}

HttpBackend::~HttpBackend() {}

const std::string& HttpBackend::protocol() const {
  static const std::string value("http");
  return value;
}

bool HttpBackend::process(RequestNotes* rn) {
  if (Connection::create(this, rn)) return true;

  rn->request->log(Severity::error,
                   "HTTP proxy: Could not connect to backend %s. %s",
                   socketSpec_.str().c_str(), strerror(errno));

  return false;
}
// }}}
