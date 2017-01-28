// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/*
 * TODO:
 * - respondWith(BadGateway) on backend message protocol errors,
   - FIXME: hanging connection in read state without idle timer for over 1day
 already
     - hanging in reading-request (header-value)
     - GET /chat/stats (fastcgi), refs:1, fd:95, timer:none, io.watcher:read
     - [01/Jul/2014:11:15:46 +0000] [error] [worker/0] [77.22.104.71]
 (message-begin) Failed to read from client. Connection reset by peer
 */

#include "FastCgiBackend.h"
#include "FastCgiHealthMonitor.h"
#include "Director.h"

#include <base/CustomDataMgr.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpConnection.h>
#include <base/io/BufferSource.h>
#include <base/io/BufferRefSource.h>
#include <base/io/FileSource.h>
#include <base/io/BufferSink.h>
#include <base/strutils.h>
#include <base/Process.h>
#include <base/Buffer.h>
#include <base/Types.h>
#include <base/StackTrace.h>
#include <base/Utility.h>
#include <x0d/sysconfig.h>

#include <system_error>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <string>
#include <deque>
#include <cctype>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#if !defined(NDEBUG)
#define TRACE(level, msg...)                                         \
  {                                                                  \
    static_assert((level) >= 1 && (level) <= 5,                      \
                  "TRACE()-level must be between 1 and 5, matching " \
                  "Severity::traceN values.");                       \
    log(Severity::trace##level, msg);                                \
  }
#else
#define TRACE(msg...) /*!*/
#endif

using namespace base;
using namespace xzero;

std::atomic<unsigned long long> transportIds_(0);

class FastCgiBackend::Connection :  // {{{
                                    public CustomData,
                                    public HttpMessageParser {
  class ParamReader : public FastCgi::CgiParamStreamReader  //{{{
                      {
   private:
    FastCgiBackend::Connection* tx_;

   public:
    explicit ParamReader(FastCgiBackend::Connection* tx) : tx_(tx) {}

    void onParam(const char* nameBuf, size_t nameLen, const char* valueBuf,
                 size_t valueLen) override {
      std::string name(nameBuf, nameLen);
      std::string value(valueBuf, valueLen);

      tx_->onParam(name, value);
    }
  };  //}}}
 public:
  explicit Connection(RequestNotes* rn, std::unique_ptr<Socket>&& backend);
  ~Connection();

 private:
  FastCgiBackend& backend() const { return *backend_; }
  std::string backendName() const { return socket_->remote(); }

  // client-request management
  void initialize();
  inline void serializeRequest();
  void exitSuccess();
  void exitFailure(HttpStatus status);
  void onClientAbort();

  // backend write ops
  template <typename T, typename... Args>
  void write(Args&&... args);
  void write(FastCgi::Type type, int requestId, Buffer&& content);
  void write(FastCgi::Type type, int requestId, const char* buf, size_t len);
  void write(FastCgi::Record* record);
  void flush();

  // backend I/O events
  void onConnectTimeout(Socket* s);
  void onConnectComplete(Socket* s, int revents);
  void onReadWriteTimeout(Socket* s);
  void onReadWriteReady(Socket* s, int revents);
  void onWriteComplete();

  // backend response handlers
  inline bool processRecord(const FastCgi::Record* record);
  void onParam(const std::string& name, const std::string& value);
  void onStdOut(const BufferRef& chunk);
  void onStdErr(const BufferRef& chunk);
  void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

  // HTTP response processing
  bool onMessageHeader(const BufferRef& name,
                       const BufferRef& value) override;
  bool onMessageHeaderEnd() override;
  bool onMessageContent(const BufferRef& content) override;

  // proxy logging
  template <typename... Args>
  void log(Severity severity, const char* fmt, Args&&... args);

  // debugging
  void inspect(Buffer& out);

 public:
  unsigned long long transportId_;  //!< unique backend connection ID
  bool isAborted_;                  //!< just for debugging right now.
  FastCgiBackend* backend_;         //!< object owner

  uint16_t id_;                         //!< request ID inside the connection
  std::unique_ptr<Socket> socket_;  //!< actual socket to backend

  Buffer readBuffer_;  //!< backend response buffer
  size_t readOffset_;

  Buffer writeBuffer_;  //!< backend request buffer
  size_t writeOffset_;      //!< write offset into the backend request buffer

  bool flushPending_;  //!< true if "pending" bytes shall be flushed, false if
                       //not.

  RequestNotes* rn_;  //!< current client request to proxy for

  char transferPath_[1024];
  int transferHandle_;  //!< file descriptor to temporary response body handle
  int transferOffset_;  //!< offset of the last client-write operatin into the
                        //@p transferHandle_.

  std::string sendfile_;  //!< path to the file to send to the client instead of
                          //the backend's response.
};                        // }}}
// {{{ FastCgiBackend::Connection impl
FastCgiBackend::Connection::Connection(
    RequestNotes* rn, std::unique_ptr<Socket>&& backendSocket)
    : HttpMessageParser(HttpMessageParser::MESSAGE),
      transportId_(++transportIds_),
      isAborted_(false),
      backend_(static_cast<FastCgiBackend*>(rn->backend)),
      id_(1),
      socket_(std::move(backendSocket)),
      readBuffer_(),
      readOffset_(0),
      writeBuffer_(),
      writeOffset_(0),
      flushPending_(false),
      rn_(rn),
      transferPath_(),
      transferHandle_(-1),
      transferOffset_(0),
      sendfile_() {
  transferPath_[0] = '\0';

  TRACE(1, "create");

  initialize();
}

FastCgiBackend::Connection::~Connection() {
  // TRACE(1, "closing transport connection to backend server.");

  if (transferPath_[0] != '\0') {
    unlink(transferPath_);
  }

  if (!(transferHandle_ < 0)) {
    ::close(transferHandle_);
  }
}

/**
 * @brief binds the given request to this FastCGI transport connection.
 *
 * @param r the HTTP client request to bind to this FastCGI transport
 *connection.
 *
 * Requests bound to a FastCGI transport will be passed to the connected
 * transport backend and served by it.
 */
void FastCgiBackend::Connection::initialize() {
  auto r = rn_->request;

  // initialize object
  r->setAbortHandler(
      std::bind(&FastCgiBackend::Connection::onClientAbort, this));
  r->registerInspectHandler<FastCgiBackend::Connection,
                            &FastCgiBackend::Connection::inspect>(this);

  serializeRequest();

  // setup I/O callback
  if (socket_->state() == Socket::Connecting) {
    socket_->setTimeout<FastCgiBackend::Connection,
                        &FastCgiBackend::Connection::onConnectTimeout>(
        this, backend_->manager()->connectTimeout());
    socket_->setReadyCallback<FastCgiBackend::Connection,
                              &FastCgiBackend::Connection::onConnectComplete>(
        this);
  } else {
    socket_->setReadyCallback<FastCgiBackend::Connection,
                              &FastCgiBackend::Connection::onReadWriteReady>(
        this);
  }

  // flush out
  flush();

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

void FastCgiBackend::Connection::serializeRequest() {
  auto r = rn_->request;

  // initialize stream
  write<FastCgi::BeginRequestRecord>(FastCgi::Role::Responder, id_, true);

  FastCgi::CgiParamStreamWriter params;
  params.encode("SERVER_SOFTWARE", PACKAGE_NAME "/" PACKAGE_VERSION);
  params.encode("SERVER_NAME", r->requestHeader("Host"));
  params.encode("GATEWAY_INTERFACE", "CGI/1.1");

  params.encode("SERVER_PROTOCOL", "1.1");
  params.encode("SERVER_ADDR", r->connection.localIP().str());
  params.encode("SERVER_PORT",
                std::to_string(r->connection.localPort()));  // TODO this should
                                                             // to be itoa'd
                                                             // only ONCE

  params.encode("REQUEST_METHOD", r->method);
  params.encode("REDIRECT_STATUS", "200");  // for PHP configured with
                                            // --force-redirect (Gentoo/Linux
                                            // e.g.)

  r->updatePathInfo();  // should we invoke this explicitely? I'd vote for no...
                        // however.

  params.encode("PATH_INFO", r->pathinfo);

  if (!r->pathinfo.empty()) {
    params.encode("PATH_TRANSLATED", r->documentRoot, r->pathinfo);
    params.encode("SCRIPT_NAME",
                  r->path.ref(0, r->path.size() - r->pathinfo.size()));
  } else {
    params.encode("SCRIPT_NAME", r->path);
  }

  params.encode("QUERY_STRING", r->query);  // unparsed uri
  params.encode("REQUEST_URI", r->unparsedUri);

  // params.encode("REMOTE_HOST", "");  // optional
  params.encode("REMOTE_ADDR", r->connection.remoteIP().str());
  params.encode("REMOTE_PORT", std::to_string(r->connection.remotePort()));

  // params.encode("REMOTE_IDENT", "");
  // params.encode("AUTH_TYPE", ""); // TODO

  if (!r->username.empty()) params.encode("REMOTE_USER", r->username);

  if (r->body()) {
    params.encode("CONTENT_TYPE", r->requestHeader("Content-Type"));
    params.encode("CONTENT_LENGTH", r->requestHeader("Content-Length"));
  }

  if (r->connection.isSecure()) params.encode("HTTPS", "on");

  // HTTP request headers
  for (const auto& header : r->requestHeaders) {
    std::string key;
    key.reserve(5 + header.name.size());
    key += "HTTP_";

    for (auto p = header.name.begin(), q = header.name.end(); p != q; ++p)
      key += std::isalnum(*p) ? std::toupper(*p) : '_';

    params.encode(key, header.value);
  }
  params.encode("DOCUMENT_ROOT", r->documentRoot);

  if (r->fileinfo) {
    params.encode("SCRIPT_FILENAME", r->fileinfo->path());
  }

  write(FastCgi::Type::Params, id_, params.output());
  write(FastCgi::Type::Params, id_, "", 0);  // EOS

  if (r->body()) {
    BufferSink sink;
    while (r->body()->sendto(sink) > 0) {
      write(FastCgi::Type::StdIn, id_, sink.buffer().data(),
            sink.buffer().size());
      sink.clear();
    }
  }
  write(FastCgi::Type::StdIn, id_, "", 0);  // EOS
}

/**
 * Terminates the current request and releases this proxy object.
 *
 * @note After this call, all field members must be treated as garbage.
 */
void FastCgiBackend::Connection::exitSuccess() {
  TRACE(1, "exitSuccess() aborted:$0", isAborted_);

  // XXX keep a copy on those variables on the stack as we are potentially
  // destroyed on the release()-call
  Backend* backend = backend_;
  RequestNotes* rn = rn_;

  if (!rn->request->status) {
    rn->request->status = HttpStatus::Ok;
  }

  // Notify director that this backend has just completed a request,
  backend->release(rn);

  // We actually served ths request, so finish() it.
  rn->request->finish();
}

/**
 * Rejects processing the current request.
 *
 * @note After this call, all field members must be treated as garbage.
 */
void FastCgiBackend::Connection::exitFailure(HttpStatus status) {
  // We failed processing this request, so reschedule
  // this request within the director and give it the chance
  // to be processed by another backend,
  // or give up when the director's request processing
  // timeout has been reached.

  // XXX keep a copy on those variables on the stack as we are potentially
  // destroyed in the middle of the call
  Backend* backend = backend_;
  RequestNotes* rn = rn_;

  // XXX explicitely clear custom data, also potentially destroying us
  rn->request->clearCustomData(backend);
  backend->reject(rn, status);
}

/**
 * Invoked when remote client connected before the response has been fully
 * transmitted.
 */
void FastCgiBackend::Connection::onClientAbort() {
  isAborted_ = true;

  switch (backend_->manager()->clientAbortAction()) {
    case ClientAbortAction::Ignore:
      log(Severity::debug, "Client closed connection early. Ignored.");
      break;
    case ClientAbortAction::Close:
      log(Severity::debug,
          "Client closed connection early. Aborting request to backend FastCGI "
          "server.");
      exitSuccess();
      break;
    case ClientAbortAction::Notify:
      log(Severity::debug,
          "Client closed connection early. Notifying backend FastCGI server.");
      write<FastCgi::AbortRequestRecord>(id_);
      flush();
      break;
    default:
      // BUG: internal server error
      break;
  }
}

template <typename T, typename... Args>
inline void FastCgiBackend::Connection::write(Args&&... args) {
  T record(args...);
  write(&record);
}

inline void FastCgiBackend::Connection::write(FastCgi::Type type, int requestId,
                                              Buffer&& content) {
  write(type, requestId, content.data(), content.size());
}

void FastCgiBackend::Connection::write(FastCgi::Type type, int requestId,
                                       const char* buf, size_t len) {
  const size_t chunkSizeCap = 0xFFFF;
  static const char padding[8] = {0};

  if (len == 0) {
    FastCgi::Record record(type, requestId, 0, 0);
    TRACE(1, "writing packet ($0) of $1 bytes to backend server.",
          record.type_str(), len);
    writeBuffer_.push_back(record.data(), sizeof(record));
    return;
  }

  for (size_t offset = 0; offset < len;) {
    size_t clen = std::min(offset + chunkSizeCap, len) - offset;
    size_t plen =
        clen % sizeof(padding) ? sizeof(padding) - clen % sizeof(padding) : 0;

    FastCgi::Record record(type, requestId, clen, plen);
    writeBuffer_.push_back(record.data(), sizeof(record));
    writeBuffer_.push_back(buf + offset, clen);
    writeBuffer_.push_back(padding, plen);

    offset += clen;

    TRACE(1, "writing packet ($0) of $1 bytes to backend server.",
          record.type_str(), record.size());
  }
}

void FastCgiBackend::Connection::write(FastCgi::Record* record) {
  TRACE(1, "writing packet ($0) of $1 bytes to backend server.",
        record->type_str(), record->size());

  writeBuffer_.push_back(record->data(), record->size());
}

void FastCgiBackend::Connection::flush() {
  if (socket_->state() == Socket::Operational) {
    TRACE(1, "flushing pending data to backend server.");
    socket_->setTimeout<FastCgiBackend::Connection,
                        &FastCgiBackend::Connection::onReadWriteTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_->setMode(Socket::ReadWrite);
  } else {
    TRACE(1, "mark pending data to be flushed to backend server.");
    flushPending_ = true;
  }
}

void FastCgiBackend::Connection::onConnectTimeout(Socket* s) {
  log(Severity::error,
      "Trying to connect to backend server %s was timing out.",
      backend_->name().c_str());

  backend_->setState(HealthState::Offline);

  exitFailure(HttpStatus::GatewayTimeout);
}

/**
 * Invoked (by open() or asynchronousely by io()) to complete the connection
 * establishment.
 */
void FastCgiBackend::Connection::onConnectComplete(Socket* s, int revents) {
  if (s->isClosed()) {
    log(Severity::error, "Connecting to backend server failed. %s",
        strerror(errno));
    exitFailure(HttpStatus::ServiceUnavailable);
  } else if (writeBuffer_.size() > writeOffset_ && flushPending_) {
    TRACE(1, "Connected. Flushing pending data.");
    flushPending_ = false;
    socket_->setTimeout<FastCgiBackend::Connection,
                        &FastCgiBackend::Connection::onReadWriteTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_->setReadyCallback<FastCgiBackend::Connection,
                              &FastCgiBackend::Connection::onReadWriteReady>(
        this);
    socket_->setMode(Socket::ReadWrite);
  } else {
    TRACE(1, "Connected.");
    // do not install a timeout handler here, even though, we're watching for
    // ev::READ, because all we're to
    // get is an EOF detection (remote end-point will not sent data unless we
    // did).
    socket_->setReadyCallback<FastCgiBackend::Connection,
                              &FastCgiBackend::Connection::onReadWriteReady>(
        this);
    socket_->setMode(Socket::Read);
  }
}

void FastCgiBackend::Connection::onReadWriteTimeout(Socket* s) {
  log(Severity::error, "I/O timeout to backend %s: %s",
      backendName().c_str(), strerror(errno));

  backend_->setState(HealthState::Offline);

  exitFailure(HttpStatus::GatewayTimeout);
}

void FastCgiBackend::Connection::onReadWriteReady(Socket* s, int revents) {
  TRACE(1, "Received I/O activity on backend socket. revents=$0", revents);

  if (revents & ev::ERROR) {
    log(Severity::error,
        "Internal error occured while waiting for I/O readiness from backend "
        "application.");
    exitFailure(HttpStatus::ServiceUnavailable);
    return;
  }

  if (revents & Socket::Read) {
    TRACE(1, "reading from backend server.");
    // read as much as possible
    for (;;) {
      size_t remaining = readBuffer_.capacity() - readBuffer_.size();
      if (remaining < 1024) {
        readBuffer_.reserve(readBuffer_.capacity() + 4 * 4096);
        remaining = readBuffer_.capacity() - readBuffer_.size();
      }

      int rv = socket_->read(readBuffer_);

      if (rv == 0) {
        if (isAborted_) {
          exitSuccess();
        } else {
          log(Severity::error, "Reading from backend %s failed: %s.",
              backendName().c_str(), strerror(errno));
          exitFailure(HttpStatus::ServiceUnavailable);
        }
        return;
      }

      if (rv < 0) {
        if (errno != EINTR && errno != EAGAIN) {  // TODO handle EWOULDBLOCK
          log(Severity::error, "Read from backend %s failed: %s",
              backendName().c_str(), strerror(errno));
          exitFailure(HttpStatus::ServiceUnavailable);
          return;
        }

        break;
      }
    }

    // process fully received records
    while (readOffset_ + sizeof(FastCgi::Record) <= readBuffer_.size()) {
      const FastCgi::Record* record = reinterpret_cast<const FastCgi::Record*>(
          readBuffer_.data() + readOffset_);

      // payload fully available?
      if (readBuffer_.size() - readOffset_ < record->size()) {
        break;
      }

      readOffset_ += record->size();

      TRACE(1, "Processing received FastCGI packet ($0).", record->type_str());

      if (!processRecord(record)) {
        return;
      }
    }
  }

  if (revents & Socket::Write) {
    ssize_t rv = socket_->write(writeBuffer_.ref(writeOffset_));

    if (rv < 0) {
      if (errno != EINTR && errno != EAGAIN) {
        log(Severity::error, "Writing to backend %s failed: %s",
            backendName().c_str(), strerror(errno));
        exitFailure(HttpStatus::ServiceUnavailable);
      }

      return;
    }

    writeOffset_ += rv;

    TRACE(1, "Wrote $0 bytes to backend server.", rv);

    // if set watcher back to EV_READ if the write-buffer has been fully written
    // (to catch connection close events)
    if (writeOffset_ == writeBuffer_.size()) {
      TRACE(1, "Pending write-buffer fully flushed to upstraem server.");
      socket_->setMode(Socket::Read);
      writeBuffer_.clear();
      writeOffset_ = 0;
    }
  }
}

/**
 * write-completion hook, invoked when a content chunk is written to the HTTP
 * client.
 */
void FastCgiBackend::Connection::onWriteComplete() {
#if 0   //{{{
    TRACE(1, "FastCgiBackend::Connection.onWriteComplete() bufferSize:$0d",
          writeBuffer_.size());

    if (writeBuffer_.size() != 0) {
        TRACE(1, "onWriteComplete: queued:$0", writeBuffer_.size());

        auto r = rn_->request;

        r->write<BufferSource>(std::move(writeBuffer_));

        if (r->connection.isOutputPending()) {
            TRACE(1, "onWriteComplete: output pending. enqueue callback");
            r->writeCallback<FastCgiBackend::Connection, &FastCgiBackend::Connection::onWriteComplete>(this);
            return;
        }
    }
#endif  //}}}
  TRACE(1,
        "onWriteComplete: output flushed. resume watching on read events. "
        "isAborted: %s, sockOpen: %s",
        isAborted_ ? "yes" : "no", socket_->isOpen() ? "yes" : "no");

  if (!socket_->isOpen()) {
    return;
  }

  // the connection to the backend may already have been closed here when
  // we sent out BIG data to the client and the backend server has issued an
  // EndRequest-event already,
  // which causes a close() on this object and thus closes the connection to
  // the backend server already, even though not all data has been flushed out
  // to the client yet.

  TRACE(1, "Writing to client completed. Resume watching on app I/O for read.");
  socket_->setTimeout<FastCgiBackend::Connection,
                      &FastCgiBackend::Connection::onReadWriteTimeout>(
      this, backend_->manager()->readTimeout());
  socket_->setMode(Socket::Read);
}

bool FastCgiBackend::Connection::processRecord(const FastCgi::Record* record) {
  TRACE(
      1,
      "processRecord(type=%s (%d), rid=%d, contentLength=%d, paddingLength=%d)",
      record->type_str(), record->type(), record->requestId(),
      record->contentLength(), record->paddingLength());

  switch (record->type()) {
    case FastCgi::Type::GetValuesResult:
      ParamReader(this)
          .processParams(record->content(), record->contentLength());
      break;
    case FastCgi::Type::StdOut:
      onStdOut(readBuffer_.ref(record->content() - readBuffer_.data(),
                               record->contentLength()));
      break;
    case FastCgi::Type::StdErr:
      onStdErr(readBuffer_.ref(record->content() - readBuffer_.data(),
                               record->contentLength()));
      break;
    case FastCgi::Type::EndRequest:
      onEndRequest(
          static_cast<const FastCgi::EndRequestRecord*>(record)->appStatus(),
          static_cast<const FastCgi::EndRequestRecord*>(record)
              ->protocolStatus());
      return false;
    case FastCgi::Type::UnknownType:
    default:
      log(Severity::error,
          "Unknown transport record received from backend %s. type:%d, "
          "payload-size:%ld",
          backendName().c_str(), record->type(), record->contentLength());
#if 1
      Buffer::dump(record, sizeof(record), "fcgi packet header");
      Buffer::dump(
          record->content(),
          std::min(record->contentLength() + record->paddingLength(), 512),
          "fcgi packet payload");
#endif
      break;
  }
  return true;
}

void FastCgiBackend::Connection::onParam(const std::string& name,
                                         const std::string& value) {
  TRACE(1, "Received protocol parameter $0=$1.", name, value);
}

void FastCgiBackend::Connection::onStdOut(const BufferRef& chunk) {
  TRACE(1, "Received $0 bytes from backend server (state=$1).",
        chunk.size(), state());
  // TRACE(2, "data: $0", chunk.str());

  parseFragment(chunk);
}

inline std::string chomp(const std::string& value)  // {{{
{
  if (value.size() && value[value.size() - 1] == '\n')
    return value.substr(0, value.size() - 1);
  else
    return value;
}  // }}}

void FastCgiBackend::Connection::onStdErr(const BufferRef& chunk) {
  log(Severity::error, "%s", chomp(chunk.str()).c_str());
}

void FastCgiBackend::Connection::onEndRequest(
    int appStatus, FastCgi::ProtocolStatus protocolStatus) {
  TRACE(1,
        "Received EndRequest-event from backend server (appStatus=%d "
        "protocolStatus=%d). Closing transport.",
        appStatus, static_cast<int>(protocolStatus));

  switch (protocolStatus) {
    case FastCgi::ProtocolStatus::RequestComplete:
      exitSuccess();
      break;
    case FastCgi::ProtocolStatus::CannotMpxConnection:
      log(Severity::error,
          "Backend appliation terminated request because it says it cannot "
          "multiplex connections.");
      exitFailure(HttpStatus::InternalServerError);
      break;
    case FastCgi::ProtocolStatus::Overloaded:
      log(Severity::error,
          "Backend appliation terminated request because it says it is "
          "overloaded.");
      exitFailure(HttpStatus::ServiceUnavailable);
      break;
    case FastCgi::ProtocolStatus::UnknownRole:
      log(Severity::error,
          "Backend appliation terminated request because it cannot handle this "
          "role.");
      exitFailure(HttpStatus::InternalServerError);
      break;
    default:
      log(Severity::error,
          "Backend appliation terminated request with unknown error code %d.",
          static_cast<int>(protocolStatus));
      exitFailure(HttpStatus::InternalServerError);
      break;
  }
}

bool FastCgiBackend::Connection::onMessageHeader(const BufferRef& name,
                                                 const BufferRef& value) {
  TRACE(1, "parsed HTTP header from backend server. $0: $1",
        name.str(), value.str());

  if (iequals(name, "Status")) {
    int status = value.ref(0, value.find(' ')).toInt();
    rn_->request->status = static_cast<HttpStatus>(status);
  } else if (iequals(name, "X-Sendfile")) {
    sendfile_ = value.str();
  } else {
    if (name == "Location")
      rn_->request->status = HttpStatus::MovedTemporarily;

    rn_->request->responseHeaders.push_back(name.str(), value.str());
  }

  return true;
}

bool FastCgiBackend::Connection::onMessageHeaderEnd() {
  TRACE(1, "onMessageHeaderEnd()");

  if (unlikely(!sendfile_.empty())) {
    auto r = rn_->request;
    r->responseHeaders.remove("Content-Type");
    r->responseHeaders.remove("Content-Length");
    r->responseHeaders.remove("ETag");
    r->sendfile(sendfile_);
  }

  return true;
}

bool FastCgiBackend::Connection::onMessageContent(const BufferRef& chunk) {
  auto r = rn_->request;

  TRACE(1, "Parsed HTTP message content of $0 bytes from backend server.",
        chunk.size());
  // TRACE(2, "Message content chunk: $0", chunk.str());

  if (unlikely(!sendfile_.empty())) {
    // we ignore the backend's message body as we've replaced it with the file
    // contents of X-Sendfile's file.
    return true;
  }

  if (!(transferHandle_ < 0)) {
    ssize_t rv = ::write(transferHandle_, chunk.data(), chunk.size());
    if (rv > 0) {  // something written?
      r->write<FileSource>(transferHandle_, transferOffset_, rv, false);
      transferOffset_ += rv;

      if (rv != static_cast<ssize_t>(chunk.size())) {
        // partial disk-write, so complete it with a memory-write fallback
        r->write<BufferRefSource>(chunk.ref(rv));
      }

      return true;
    }
  }

  r->write<BufferRefSource>(chunk);
  return true;
}

template <typename... Args>
inline void FastCgiBackend::Connection::log(Severity severity, const char* fmt,
                                            Args&&... args) {
  if (rn_) {
    rn_->request->log(severity, fmt, std::forward<Args>(args)...);
  }
}

void FastCgiBackend::Connection::inspect(Buffer& out) {
  out << "aborted:" << isAborted_ << ", ";
  if (rn_) {
    out << "isOutputPending:" << rn_->request->connection.isOutputPending()
        << ", ";
  } else {
    out << "no-request-bound, ";
  }

  socket_->inspect(out);
}
// }}}
// {{{ FastCgiBackend impl
FastCgiBackend::FastCgiBackend(BackendManager* bm, const std::string& name,
                               const SocketSpec& socketSpec, size_t capacity,
                               bool healthChecks)
    : Backend(bm, name, socketSpec, capacity,
              healthChecks ? std::make_unique<FastCgiHealthMonitor>(
                                 *bm->worker()->server().nextWorker())
                           : nullptr) {
  if (healthChecks) {
    healthMonitor()->setBackend(this);
  }
}

FastCgiBackend::~FastCgiBackend() {}

const std::string& FastCgiBackend::protocol() const {
  static const std::string value("fastcgi");
  return value;
}

bool FastCgiBackend::process(RequestNotes* rn) {
  // TRACE(1, "process()");

  std::unique_ptr<Socket> socket(
      Socket::open(rn->request->connection.worker().loop(), socketSpec_,
                       O_NONBLOCK | O_CLOEXEC));

  if (socket) {
    assert(rn->backend == this);
    rn->request->setCustomData<Connection>(this, rn, std::move(socket));
    return true;
  } else {
    rn->request->log(Severity::notice,
                     "fastcgi: connection to backend %s failed (%d). %s",
                     socketSpec_.str().c_str(), errno, strerror(errno));
    return false;
  }
}
//}}}
