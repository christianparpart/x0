// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "FastCgiHealthMonitor.h"
#include "FastCgiProtocol.h"
#include "Backend.h"
#include "Director.h"
#include <cstdarg>

/*
 * TODO
 * - connect/read/write timeout handling.
 */

using namespace base;
using namespace xzero;

#if !defined(NDEBUG)
#define TRACE(msg...) (this->debug(msg))
#else
#define TRACE(msg...) \
  do {                \
  } while (0)
#endif

FastCgiHealthMonitor::FastCgiHealthMonitor(HttpWorker& worker)
    : HealthMonitor(worker, HttpMessageParser::MESSAGE),
      socket_(worker_.loop()),
      writeBuffer_(),
      writeOffset_(0),
      readBuffer_(),
      readOffset_(0) {}

FastCgiHealthMonitor::~FastCgiHealthMonitor() {
  TRACE("~FastCgiHealthMonitor");
}

void FastCgiHealthMonitor::reset() {
  TRACE("reset()");

  HealthMonitor::reset();

  socket_.close();

  writeOffset_ = 0;
  readOffset_ = 0;
  readBuffer_.clear();
}

static inline std::string hostname() {
  char buf[256];
  if (gethostname(buf, sizeof(buf)) == 0) return buf;

  return "localhost";
}

// {{{ HttpRequestRec
class HttpRequestRec : public HttpMessageParser {
 public:
  BufferRef method;
  BufferRef path;
  std::list<std::pair<BufferRef, BufferRef>> headers;

  static HttpRequestRec parse(const BufferRef& request) {
    HttpRequestRec rr;

    rr.parseFragment(request);

    return rr;
  }

 private:
  HttpRequestRec()
      : HttpMessageParser(HttpMessageParser::REQUEST),
        method(),
        path(),
        headers() {}

  virtual bool onMessageBegin(const BufferRef& method,
                              const BufferRef& entity, int versionMajor,
                              int versionMinor) {
    this->method = method;
    path = entity;

    return true;
  }

  virtual bool onMessageHeader(const BufferRef& name,
                               const BufferRef& value) {
    headers.push_back(std::make_pair(name, value));

    return true;
  }

  virtual void log(LogMessage&& msg) {
    // TODO: in order to actually implement me, you must also pass the monitor I
    // am working for
  }
};
// }}}

/**
 * Sets the raw HTTP request, used to perform the health check.
 */
void FastCgiHealthMonitor::setRequest(const char* fmt, ...) {
  // XXX parse HTTP request message

  va_list va;
  size_t blen = 1023lu;
  Buffer request;

  do {
    request.reserve(blen + 1);
    va_start(va, fmt);
    blen = vsnprintf(const_cast<char*>(request.data()), request.capacity(), fmt,
                     va);
    va_end(va);
  } while (blen >= request.capacity());

  request.resize(blen);

  // XXX actually parse `request` and pre-fill writeBuffer_
  HttpRequestRec rr = HttpRequestRec::parse(request.ref());
  FastCgi::CgiParamStreamWriter params;

  params.encode("GATEWAY_INTERFACE", "CGI/1.1");
  params.encode("SERVER_NAME", hostname());
  params.encode("SERVER_PORT", "0");  // this is an artificial request
  params.encode("SERVER_PROTOCOL", "1.1");
  params.encode("SERVER_SOFTWARE", PACKAGE_NAME "/" PACKAGE_VERSION);
  params.encode("REQUEST_METHOD", rr.method.str());
  params.encode("SCRIPT_NAME", rr.path.str());

  // XXX we know we are only part of a Director backend-manager
  if (!static_cast<Director*>(backend_->manager())
           ->healthCheckFcgiScriptFilename()
           .empty())
    params.encode("SCRIPT_FILENAME", static_cast<Director*>(backend_->manager())
                                         ->healthCheckFcgiScriptFilename());

  for (auto& header : rr.headers) {
    std::string key;
    key.reserve(5 + header.first.size());
    key += "HTTP_";

    for (auto p = header.first.begin(), q = header.first.end(); p != q; ++p)
      key += std::isalnum(*p) ? std::toupper(*p) : '_';

    params.encode(key, header.second);
  }

  writeBuffer_.clear();
  write<FastCgi::BeginRequestRecord>(FastCgi::Role::Responder, 1, true);
  write(FastCgi::Type::Params, params.output());
  write(FastCgi::Type::Params, Buffer());  // EOS
}

template <typename T, typename... Args>
inline void FastCgiHealthMonitor::write(Args&&... args) {
  T record(args...);

  TRACE("write(type=$0, rid=$1, size=$2, pad=$3)",
        record.type_str(), record.requestId(), record.size(),
        record.paddingLength());

  writeBuffer_.push_back(record.data(), record.size());
}

void FastCgiHealthMonitor::write(FastCgi::Type type, const Buffer& buffer) {
  const int requestId = 1;
  const size_t chunkSizeCap = 0xFFFF;
  static const char padding[8] = {0};

  if (buffer.empty()) {
    FastCgi::Record record(type, requestId, 0, 0);
    TRACE("write(type=$0, rid=$1, size=0)",
          record.type_str(), requestId);
    writeBuffer_.push_back(record.data(), sizeof(record));
    return;
  }

  for (size_t offset = 0; offset < buffer.size();) {
    size_t clen = std::min(offset + chunkSizeCap, buffer.size()) - offset;
    size_t plen =
        clen % sizeof(padding) ? sizeof(padding) - clen % sizeof(padding) : 0;

    FastCgi::Record record(type, requestId, clen, plen);
    writeBuffer_.push_back(record.data(), sizeof(record));
    writeBuffer_.push_back(buffer.data() + offset, clen);
    writeBuffer_.push_back(padding, plen);

    TRACE("write(type=$0, rid=$1, offset=$2, size=$3, plen=$4)",
          record.type_str(), requestId, offset, clen, plen);

    offset += clen;
  }
}

/**
 * Callback, timely invoked when a health check is to be started.
 */
void FastCgiHealthMonitor::onCheckStart() {
  TRACE("onCheckStart()");

  socket_.open(backend_->socketSpec(), O_NONBLOCK | O_CLOEXEC);

  if (!socket_.isOpen()) {
    TRACE("Connect failed. $0", strerror(errno));
    logFailure();
  } else if (socket_.state() == Socket::Connecting) {
    TRACE("connecting asynchronously.");
    socket_.setTimeout<FastCgiHealthMonitor, &FastCgiHealthMonitor::onTimeout>(
        this, backend_->manager()->connectTimeout());
    socket_.setReadyCallback<FastCgiHealthMonitor,
                             &FastCgiHealthMonitor::onConnectDone>(this);
    socket_.setMode(Socket::ReadWrite);
  } else {
    socket_.setTimeout<FastCgiHealthMonitor, &FastCgiHealthMonitor::onTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_.setReadyCallback<FastCgiHealthMonitor, &FastCgiHealthMonitor::io>(
        this);
    socket_.setMode(Socket::ReadWrite);
    TRACE("connected.");
  }
}

/**
 * Callback, invoked on completed asynchronous connect-operation.
 */
void FastCgiHealthMonitor::onConnectDone(Socket*, int revents) {
  TRACE("onConnectDone($0)", revents);

  if (socket_.state() == Socket::Operational) {
    TRACE("connected");
    socket_.setTimeout<FastCgiHealthMonitor, &FastCgiHealthMonitor::onTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_.setReadyCallback<FastCgiHealthMonitor, &FastCgiHealthMonitor::io>(
        this);
    socket_.setMode(Socket::ReadWrite);
  } else {
    TRACE("Asynchronous connect failed $0", strerror(errno));
    logFailure();
  }
}

/**
 * Callback, invoked on I/O readiness of origin server connection.
 */
void FastCgiHealthMonitor::io(Socket*, int revents) {
  TRACE("io($0)", revents);

  if (revents & ev::WRITE) {
    if (!writeSome()) {
      return;
    }
  }

  if (revents & ev::READ) {
    if (!readSome()) {
      return;
    }
  }
}

/**
 * Writes the request chunk to the origin server.
 */
bool FastCgiHealthMonitor::writeSome() {
  TRACE("writeSome()");

  size_t chunkSize = writeBuffer_.size() - writeOffset_;
  ssize_t writeCount =
      socket_.write(writeBuffer_.data() + writeOffset_, chunkSize);

  if (writeCount < 0) {
    TRACE("write failed. $0", strerror(errno));
    logFailure();
  } else {
    writeOffset_ += writeCount;

    if (writeOffset_ == writeBuffer_.size()) {
      socket_
          .setTimeout<FastCgiHealthMonitor, &FastCgiHealthMonitor::onTimeout>(
              this, backend_->manager()->readTimeout());
      socket_.setMode(Socket::Read);
    }
  }

  return true;
}

/**
 * Reads and processes a response chunk from origin server.
 */
bool FastCgiHealthMonitor::readSome() {
  TRACE("readSome()");

  // read as much as possible
  for (;;) {
    size_t remaining = readBuffer_.capacity() - readBuffer_.size();
    if (remaining < 1024) {
      readBuffer_.reserve(readBuffer_.capacity() + 4 * 4096);
      remaining = readBuffer_.capacity() - readBuffer_.size();
    }

    int rv = socket_.read(readBuffer_);

    if (rv == 0) {
      worker_.log(Severity::error, "fastcgi: connection to backend lost.");
      logFailure();
      return false;
    }

    if (rv < 0) {
      if (errno != EINTR && errno != EAGAIN) {  // TODO handle EWOULDBLOCK
        worker_.log(Severity::error,
                    "fastcgi: read from backend %s failed: %s",
                    backend_->socketSpec().str().c_str(), strerror(errno));
        logFailure();
        return false;
      }

      break;
    }
  }

  TRACE("readSome: read $0 bytes", readBuffer_.size() - readOffset_);

  // process fully received records
  while (readOffset_ + sizeof(FastCgi::Record) <= readBuffer_.size()) {
    const FastCgi::Record* record = reinterpret_cast<const FastCgi::Record*>(
        readBuffer_.data() + readOffset_);

    // payload fully available?
    if (readBuffer_.size() - readOffset_ < record->size()) break;

    readOffset_ += record->size();

    if (!processRecord(record)) return true;
  }

  socket_.setTimeout<FastCgiHealthMonitor, &FastCgiHealthMonitor::onTimeout>(
      this, backend_->manager()->readTimeout());

  return true;
}

/**
 * Processes a single FastCGI packet.
 *
 * \retval true continue processing more packets, please.
 * \retval false do NOT continue processing further packets!
 */
bool FastCgiHealthMonitor::processRecord(const FastCgi::Record* record) {
  TRACE(
      "processRecord(type=%s (%d), rid=%d, contentLength=%d, paddingLength=%d)",
      record->type_str(), record->type(), record->requestId(),
      record->contentLength(), record->paddingLength());

  bool proceedHint = true;

  switch (record->type()) {
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
      proceedHint = false;
      break;
    case FastCgi::Type::GetValuesResult:
    case FastCgi::Type::UnknownType:
    default:
      worker_.log(Severity::error,
                  "fastcgi: unknown transport record received from backend %s. "
                  "type:%d, payload-size:%ld",
                  backend_->socketSpec().str().c_str(), record->type(),
                  record->contentLength());
#if 1
      Buffer::dump(record, sizeof(record), "fcgi packet header");
      Buffer::dump(
          record->content(),
          std::min(record->contentLength() + record->paddingLength(), 512),
          "fcgi packet payload");
#endif
      break;
  }
  return proceedHint;
}

void FastCgiHealthMonitor::onStdOut(const BufferRef& chunk) {
  TRACE("onStdOut: chunk.size=$0", chunk.size());
  parseFragment(chunk);
}

void FastCgiHealthMonitor::onStdErr(const BufferRef& chunk) {
  worker_.log(Severity::error, "fastcgi: Health check error. %s",
              chunk.chomp().str().c_str());
}

void FastCgiHealthMonitor::onEndRequest(
    int appStatus, FastCgi::ProtocolStatus protocolStatus) {
  TRACE("onEndRequest(appStatus=$0, protocolStatus=$1)",
        appStatus, protocolStatus);

  // explicitely invoke HttpMessageParser hook since ParseMode::MESSAGE doesn't
  // invoke it in this mode.

  // some FastCGI backends (i.e. php-fpm) do not always sent a Status response
  // header
  // in order to tell us their response status code, so we default to 200 (Ok)
  // here, if
  // and only if the application's status code is 0 (which usually means
  // Success, too).

  if (responseCode_ == HttpStatus::Undefined &&
      protocolStatus == FastCgi::ProtocolStatus::RequestComplete &&
      appStatus == 0) {
    responseCode_ = HttpStatus::Ok;
  }

  onMessageEnd();
}

/**
 * Origin server timed out in read or write operation.
 */
void FastCgiHealthMonitor::onTimeout(Socket* s) {
  TRACE("onTimeout()");
  logFailure();
}
