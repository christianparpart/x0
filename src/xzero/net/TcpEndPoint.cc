// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/net/Connection.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/io/FileUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <xzero/RefPtr.h>
#include <stdexcept>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace xzero {

#define ERROR(msg...) logError("net.TcpEndPoint", msg)

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("net.TcpEndPoint", msg)
#define DEBUG(msg...) logDebug("net.TcpEndPoint", msg)
#else
#define TRACE(msg...) do {} while (0)
#define DEBUG(msg...) do {} while (0)
#endif

TcpEndPoint::TcpEndPoint(int socket,
                          int addressFamily,
                          Duration readTimeout,
                          Duration writeTimeout,
                          Executor* executor,
                          std::function<void(TcpEndPoint*)> onEndPointClosed)
    : io_(),
      onEndPointClosed_(onEndPointClosed),
      executor_(executor),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      inputBuffer_(),
      inputOffset_(0),
      handle_(socket),
      addressFamily_(addressFamily),
      isCorking_(false) {
  TRACE("$0 ctor", this);
}

void TcpEndPoint::onTimeout() {
  if (connection()) {
    if (connection()->onReadTimeout()) {
      close();
    }
  }
}

TcpEndPoint::~TcpEndPoint() {
  TRACE("$0 dtor", this);
  if (isOpen()) {
    close();
  }
}

Option<InetAddress> TcpEndPoint::remoteAddress() const {
  Result<InetAddress> addr = TcpUtil::getRemoteAddress(handle_, addressFamily());
  if (addr.isSuccess())
    return Some(*addr);
  else {
    logError("TcpEndPoint", "remoteAddress: ($0) $1",
        addr.error().category().name(),
        addr.error().message().c_str());
    return None();
  }
}

Option<InetAddress> TcpEndPoint::localAddress() const {
  Result<InetAddress> addr = TcpUtil::getLocalAddress(handle_, addressFamily());
  if (addr.isSuccess())
    return Some(*addr);
  else {
    logError("TcpEndPoint", "localAddress: ($0) $1",
        addr.error().category().name(),
        addr.error().message().c_str());
    return None();
  }
}

bool TcpEndPoint::isOpen() const XZERO_NOEXCEPT {
  return handle_ >= 0;
}

void TcpEndPoint::close() {
  if (isOpen()) {
    TRACE("close() fd=$0", handle_);
    FileUtil::close(handle_);
    handle_ = -1;

    if (onEndPointClosed_) {
      onEndPointClosed_(this);
    }
  } else {
    TRACE("close(fd=$0) invoked, but we're closed already", handle_);
  }
}

void TcpEndPoint::setConnection(std::unique_ptr<Connection>&& c) {
  connection_ = std::move(c);
}

bool TcpEndPoint::isBlocking() const {
  return !(fcntl(handle_, F_GETFL) & O_NONBLOCK);
}

void TcpEndPoint::setBlocking(bool enable) {
  FileUtil::setBlocking(handle_, enable);
}

bool TcpEndPoint::isCorking() const {
  return isCorking_;
}

void TcpEndPoint::setCorking(bool enable) {
  if (isCorking_ != enable) {
    TcpUtil::setCorking(handle_, enable);
    isCorking_ = enable;
  }
}

bool TcpEndPoint::isTcpNoDelay() const {
  return TcpUtil::isTcpNoDelay(handle_);
}

void TcpEndPoint::setTcpNoDelay(bool enable) {
  TcpUtil::setTcpNoDelay(handle_, enable);
}

std::string TcpEndPoint::toString() const {
  char buf[32];
  snprintf(buf, sizeof(buf), "TcpEndPoint(%d)@%p", handle(), this);
  return buf;
}

void TcpEndPoint::startDetectProtocol(
    bool dataReady,
    std::function<void(const std::string&, TcpEndPoint*)> createConnection) {
  inputBuffer_.reserve(256);

  if (dataReady) {
    onDetectProtocol(createConnection);
  } else {
    executor_->executeOnReadable(
        handle(),
        std::bind(&TcpEndPoint::onDetectProtocol, this, createConnection));
  }
}

void TcpEndPoint::onDetectProtocol(
    std::function<void(const std::string&, TcpEndPoint*)> createConnection) {
  size_t n = fill(&inputBuffer_);

  if (n == 0) {
    close();
    return;
  }

  // XXX detect magic byte (0x01) to protocol detection
  if (inputBuffer_[0] == TcpConnector::MagicProtocolSwitchByte) {
    BinaryReader reader(inputBuffer_);
    reader.parseVarUInt(); // skip magic
    std::string protocol = reader.parseString();
    inputOffset_ = inputBuffer_.size() - reader.pending();
    createConnection(protocol, this);
  } else {
    // create Connection object for given endpoint
    TRACE("$0 protocol-switch not detected.", this);
    createConnection("", this);
  }

  connection()->onOpen(true);
}

size_t TcpEndPoint::prefill(size_t maxBytes) {
  const size_t nprefilled = prefilled();
  if (nprefilled > 0)
    return nprefilled;

  inputBuffer_.reserve(maxBytes);
  return fill(&inputBuffer_);
}

size_t TcpEndPoint::fill(Buffer* sink) {
  int space = sink->capacity() - sink->size();
  if (space < 4 * 1024) {
    sink->reserve(sink->capacity() + 8 * 1024);
    space = sink->capacity() - sink->size();
  }

  return fill(sink, space);
}


size_t TcpEndPoint::fill(Buffer* result, size_t count) {
  assert(count <= result->capacity() - result->size());

  if (inputOffset_ < inputBuffer_.size()) {
    TRACE("$0 fill: with inputBuffer ($1, $2)", this, inputOffset_, inputBuffer_.size());
    count = std::min(count, inputBuffer_.size() - inputOffset_);
    result->push_back(inputBuffer_.ref(inputOffset_, count));
    TRACE("$0 fill: \"$1\"", this, inputBuffer_.ref(inputOffset_, count));
    inputOffset_ += count;
    if (inputOffset_ == inputBuffer_.size()) {
      inputBuffer_.clear();
      inputOffset_ = 0;
    }
    return count;
  }

  ssize_t n = read(handle(), result->end(), count);
  TRACE("read($0 bytes) -> $1", result->capacity() - result->size(), n);

  if (n < 0) {
    // don't raise on soft errors, such as there is simply no more data to read.
    switch (errno) {
      case EBUSY:
      case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
      case EWOULDBLOCK:
#endif
        break;
      default:
        RAISE_ERRNO(errno);
    }
  } else {
    result->resize(result->size() + n);
  }

  return n;
}

size_t TcpEndPoint::flush(const BufferRef& source) {
  ssize_t rv = write(handle(), source.data(), source.size());

  TRACE("flush($0 bytes) -> $1", source.size(), rv);

  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
}

size_t TcpEndPoint::flush(const FileView& view) {
  return TcpUtil::sendfile(handle(), view);
}

void TcpEndPoint::wantFill() {
  TRACE("$0 wantFill()", this);
  // TODO: abstract away the logic of TCP_DEFER_ACCEPT

  if (!io_) {
    io_ = executor_->executeOnReadable(
        handle(),
        std::bind(&TcpEndPoint::fillable, this),
        readTimeout(),
        std::bind(&TcpEndPoint::onTimeout, this));
  }
}

void TcpEndPoint::fillable() {
  RefPtr<TcpEndPoint> _guard(this);

  try {
    io_.reset();
    connection()->onFillable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  } catch (...) {
    connection()->onInterestFailure(
        EXCEPTION(RuntimeError, (int) Status::CaughtUnknownExceptionError,
                  StatusCategory::get()));
  }
}

void TcpEndPoint::wantFlush() {
  TRACE("$0 wantFlush() $1", this, io_.get() ? "again" : "first time");

  if (!io_) {
    io_ = executor_->executeOnWritable(
        handle(),
        std::bind(&TcpEndPoint::flushable, this),
        writeTimeout(),
        std::bind(&TcpEndPoint::onTimeout, this));
  }
}

void TcpEndPoint::flushable() {
  TRACE("$0 flushable()", this);
  RefPtr<TcpEndPoint> _guard(this);
  try {
    io_.reset();
    connection()->onFlushable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  } catch (...) {
    connection()->onInterestFailure(
        EXCEPTION(RuntimeError, (int) Status::CaughtUnknownExceptionError,
                  StatusCategory::get()));
  }
}

Duration TcpEndPoint::readTimeout() {
  return readTimeout_;
}

Duration TcpEndPoint::writeTimeout() {
  return writeTimeout_;
}

void TcpEndPoint::setReadTimeout(Duration timeout) {
  readTimeout_ = timeout;
}

void TcpEndPoint::setWriteTimeout(Duration timeout) {
  writeTimeout_ = timeout;
}

class TcpConnectState {
 public:
  InetAddress inet_;
  RefPtr<TcpEndPoint> ep_;
  std::function<void(RefPtr<TcpEndPoint>)> success_;
  std::function<void(std::error_code)> failure_;

  TcpConnectState(const InetAddress& inet,
                   RefPtr<TcpEndPoint> ep,
                   std::function<void(RefPtr<TcpEndPoint>)> success,
                   std::function<void(std::error_code)> failure)
      : inet_(inet), ep_(std::move(ep)), success_(success), failure_(failure) {
  }

  void onConnectComplete() {
    int val = 0;
    socklen_t vlen = sizeof(val);
    if (getsockopt(ep_->handle(), SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
      if (val == 0) {
        TRACE("$0 onConnectComplete: connected $1. $2", this, val, strerror(val));
        success_(ep_.as<TcpEndPoint>());
      } else {
        DEBUG("Connecting to $0 failed. $1", inet_, strerror(val));
        failure_(std::make_error_code(static_cast<std::errc>(val)));
      }
    } else {
      DEBUG("Connecting to $0 failed. $1", inet_, strerror(val));
      failure_(std::make_error_code(static_cast<std::errc>(val)));
    }
    delete this;
  }
};

template<>
std::string StringUtil::toString(TcpConnectState* obj) {
  return StringUtil::format("TcpConnectState[$0]", (void*) obj);
}

void TcpEndPoint::connectAsync(const InetAddress& inet,
                                Duration connectTimeout,
                                Duration readTimeout,
                                Duration writeTimeout,
                                Executor* executor,
                                std::function<void(RefPtr<TcpEndPoint>)> success,
                                std::function<void(std::error_code ec)> failure) {
  int fd = socket(inet.family(), SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    failure(std::make_error_code(static_cast<std::errc>(errno)));
    return;
  }

  RefPtr<TcpEndPoint> ep;

  try {
    TRACE("connectAsync: to $0", inet);
    ep = new TcpEndPoint(fd, inet.family(), readTimeout, writeTimeout,
                         executor, nullptr);
    ep->setBlocking(false);

    switch (inet.family()) {
      case AF_INET: {
        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = inet.family();
        saddr.sin_port = htons(inet.port());
        memcpy(&saddr.sin_addr,
               inet.ip().data(),
               inet.ip().size());

        // this connect()-call can block if fd is non-blocking
        TRACE("connectAsync: connect(ipv4)");
        if (::connect(fd, (const struct sockaddr*) &saddr, sizeof(saddr)) < 0) {
          if (errno != EINPROGRESS) {
            TRACE("connectAsync: connect() error. $0", strerror(errno));
            failure(std::make_error_code(static_cast<std::errc>(errno)));
            return;
          } else {
            TRACE("connectAsync: backgrounding");
            executor->executeOnWritable(fd,
                std::bind(&TcpConnectState::onConnectComplete,
                          new TcpConnectState(inet, ep, success, failure)));
            return;
          }
        }
        TRACE("connectAsync: synchronous connect");
        break;
      }
      case AF_INET6: {
        struct sockaddr_in6 saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin6_family = inet.family();
        saddr.sin6_port = htons(inet.port());
        memcpy(&saddr.sin6_addr,
               inet.ip().data(),
               inet.ip().size());

        // this connect()-call can block if fd is non-blocking
        if (::connect(fd, (const struct sockaddr*) &saddr, sizeof(saddr)) < 0) {
          if (errno != EINPROGRESS) {
            TRACE("connectAsync: connect() error. $0", strerror(errno));
            RAISE_ERRNO(errno);
          } else {
            TRACE("connectAsync: backgrounding");
            executor->executeOnWritable(fd,
                std::bind(&TcpConnectState::onConnectComplete,
                          new TcpConnectState(inet, ep, success, failure)));
            return;
          }
        }
        break;
      }
      default: {
        RAISE(NotImplementedError);
      }
    }

    TRACE("connectAsync: connected instantly");
    success(ep.as<TcpEndPoint>());
    return;
  } catch (...) {
    FileUtil::close(fd);
    throw;
  }
}

Future<RefPtr<TcpEndPoint>> TcpEndPoint::connectAsync(const InetAddress& inet,
                                                        Duration connectTimeout,
                                                        Duration readTimeout,
                                                        Duration writeTimeout,
                                                        Executor* executor) {
  Promise<RefPtr<TcpEndPoint>> promise;

  connectAsync(
      inet, connectTimeout, readTimeout, writeTimeout, executor,
      [=](auto ep) { promise.success(ep); },
      [=](auto ec) { promise.failure(ec); });

  return promise.future();
}

RefPtr<TcpEndPoint> TcpEndPoint::connect(
    const InetAddress& inet,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor) {
  Future<RefPtr<TcpEndPoint>> f = connectAsync(
      inet, connectTimeout, readTimeout, writeTimeout, executor);
  f.wait();

  RefPtr<TcpEndPoint> ep = f.get();

  ep.as<TcpEndPoint>()->setBlocking(true);

  return ep;
}

template<>
std::string StringUtil::toString(TcpEndPoint* ep) {
  return StringUtil::format("$0", ep->remoteAddress());
}

} // namespace xzero
