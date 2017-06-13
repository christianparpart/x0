// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/InetEndPoint.h>
#include <xzero/net/InetConnector.h>
#include <xzero/net/InetUtil.h>
#include <xzero/net/Connection.h>
#include <xzero/net/Connector.h>
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

#if defined(HAVE_SYS_SENDFILE_H)
#include <sys/sendfile.h>
#endif

namespace xzero {

#define ERROR(msg...) logError("net.InetEndPoint", msg)

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("net.InetEndPoint", msg)
#define DEBUG(msg...) logDebug("net.InetEndPoint", msg)
#else
#define TRACE(msg...) do {} while (0)
#define DEBUG(msg...) do {} while (0)
#endif

InetEndPoint::InetEndPoint(int socket,
                           InetConnector* connector,
                           Executor* executor)
    : EndPoint(),
      connector_(connector),
      executor_(executor),
      readTimeout_(connector->readTimeout()),
      writeTimeout_(connector->writeTimeout()),
      io_(),
      inputBuffer_(),
      inputOffset_(0),
      handle_(socket),
      addressFamily_(connector->addressFamily()),
      isCorking_(false) {
  TRACE("$0 ctor fd=$1", this, handle_);
}

InetEndPoint::InetEndPoint(int socket,
                           int addressFamily,
                           Duration readTimeout,
                           Duration writeTimeout,
                           Executor* executor)
    : EndPoint(),
      connector_(nullptr),
      executor_(executor),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      io_(),
      inputBuffer_(),
      inputOffset_(0),
      handle_(socket),
      addressFamily_(addressFamily),
      isCorking_(false) {
  TRACE("$0 ctor", this);
}

void InetEndPoint::onTimeout() {
  if (connection()) {
    if (connection()->onReadTimeout()) {
      close();
    }
  }
}

InetEndPoint::~InetEndPoint() {
  TRACE("$0 dtor", this);
  if (isOpen()) {
    close();
  }
}

Option<InetAddress> InetEndPoint::remoteAddress() const {
  return InetUtil::getRemoteAddress(handle_, addressFamily());
}

Option<InetAddress> InetEndPoint::localAddress() const {
  return InetUtil::getLocalAddress(handle_, addressFamily());
}

bool InetEndPoint::isOpen() const XZERO_NOEXCEPT {
  return handle_ >= 0;
}

void InetEndPoint::close() {
  if (isOpen()) {
    TRACE("close() fd=$0", handle_);
    FileUtil::close(handle_);
    handle_ = -1;

    if (connector_) {
      connector_->onEndPointClosed(this);
    }
  }
}

bool InetEndPoint::isBlocking() const {
  return !(fcntl(handle_, F_GETFL) & O_NONBLOCK);
}

void InetEndPoint::setBlocking(bool enable) {
  TRACE("$0 setBlocking(\"$1\")", this, enable);
#if 1
  unsigned current = fcntl(handle_, F_GETFL);
  unsigned flags = enable ? (current & ~O_NONBLOCK)
                          : (current | O_NONBLOCK);
#else
  unsigned flags = enable ? fcntl(handle_, F_GETFL) & ~O_NONBLOCK
                          : fcntl(handle_, F_GETFL) | O_NONBLOCK;
#endif

  if (fcntl(handle_, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
}

bool InetEndPoint::isCorking() const {
  return isCorking_;
}

void InetEndPoint::setCorking(bool enable) {
#if defined(TCP_CORK)
  if (isCorking_ != enable) {
    int flag = enable ? 1 : 0;
    if (setsockopt(handle_, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) < 0)
      RAISE_ERRNO(errno);

    isCorking_ = enable;
  }
#endif
}

bool InetEndPoint::isTcpNoDelay() const {
  int result = 0;
  socklen_t sz = sizeof(result);
  if (getsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &result, &sz) < 0)
    RAISE_ERRNO(errno);

  return result;
}

void InetEndPoint::setTcpNoDelay(bool enable) {
  int flag = enable ? 1 : 0;
  if (setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
}

std::string InetEndPoint::toString() const {
  char buf[32];
  snprintf(buf, sizeof(buf), "InetEndPoint(%d)@%p", handle(), this);
  return buf;
}

void InetEndPoint::startDetectProtocol(bool dataReady) {
  inputBuffer_.reserve(256);

  if (dataReady) {
    onDetectProtocol();
  } else {
    executor_->executeOnReadable(
        handle(),
        std::bind(&InetEndPoint::onDetectProtocol, this));
  }
}

void InetEndPoint::onDetectProtocol() {
  size_t n = fill(&inputBuffer_);

  if (n == 0) {
    close();
    return;
  }

  // XXX detect magic byte (0x01) to protocol detection
  if (inputBuffer_[0] == Connector::MagicProtocolSwitchByte) {
    BinaryReader reader(inputBuffer_);
    reader.parseVarUInt(); // skip magic
    std::string protocol = reader.parseString();
    inputOffset_ = inputBuffer_.size() - reader.pending();
    auto factory = connector_->connectionFactory(protocol);
    if (factory) {
      TRACE("$0 protocol detected. using \"$1\".", this, protocol);
      factory(connector_, this);
    } else {
      TRACE("$0 no protocol detected. using default.", this);
      connector_->defaultConnectionFactory()(connector_, this);
    }
  } else {
    // create Connection object for given endpoint
    TRACE("$0 protocol detection disabled.", this);
    connector_->defaultConnectionFactory()(connector_, this);
  }

  connection()->onOpen(true);
}

size_t InetEndPoint::fill(Buffer* result, size_t count) {
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

size_t InetEndPoint::flush(const BufferRef& source) {
  ssize_t rv = write(handle(), source.data(), source.size());

  TRACE("flush($0 bytes) -> $1", source.size(), rv);

  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
}

size_t InetEndPoint::flush(int fd, off_t offset, size_t size) {
#if defined(__APPLE__)
  off_t len = 0;
  int rv = sendfile(fd, handle(), offset, &len, nullptr, 0);
  TRACE("flush(offset:$0, size:$1) -> $2", offset, size, rv);
  if (rv < 0)
    RAISE_ERRNO(errno);

  return len;
#else
  ssize_t rv = sendfile(handle(), fd, &offset, size);
  TRACE("flush(offset:$0, size:$1) -> $2", offset, size, rv);
  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
#endif
}

void InetEndPoint::wantFill() {
  TRACE("$0 wantFill()", this);
  // TODO: abstract away the logic of TCP_DEFER_ACCEPT

  if (!io_) {
    io_ = executor_->executeOnReadable(
        handle(),
        std::bind(&InetEndPoint::fillable, this),
        readTimeout(),
        std::bind(&InetEndPoint::onTimeout, this));
  }
}

void InetEndPoint::fillable() {
  RefPtr<EndPoint> _guard(this);

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

void InetEndPoint::wantFlush() {
  TRACE("$0 wantFlush() $1", this, io_.get() ? "again" : "first time");

  if (!io_) {
    io_ = executor_->executeOnWritable(
        handle(),
        std::bind(&InetEndPoint::flushable, this),
        writeTimeout(),
        std::bind(&InetEndPoint::onTimeout, this));
  }
}

void InetEndPoint::flushable() {
  RefPtr<EndPoint> _guard(this);

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

Duration InetEndPoint::readTimeout() {
  return readTimeout_;
}

Duration InetEndPoint::writeTimeout() {
  return writeTimeout_;
}

void InetEndPoint::setReadTimeout(Duration timeout) {
  readTimeout_ = timeout;
}

void InetEndPoint::setWriteTimeout(Duration timeout) {
  writeTimeout_ = timeout;
}

class InetConnectState {
 public:
  InetAddress inet_;
  RefPtr<InetEndPoint> ep_;
  Promise<RefPtr<EndPoint>> promise_;

  InetConnectState(const InetAddress& inet,
                   RefPtr<InetEndPoint> ep,
                   const Promise<RefPtr<EndPoint>>& promise)
      : inet_(inet), ep_(std::move(ep)), promise_(promise) {
  }

  void onConnectComplete() {
    int val = 0;
    socklen_t vlen = sizeof(val);
    if (getsockopt(ep_->handle(), SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
      if (val == 0) {
        TRACE("$0 onConnectComplete: connected $1. $2", this, val, strerror(val));
        promise_.success(ep_.as<EndPoint>());
      } else {
        DEBUG("Connecting to $0 failed. $1", inet_, strerror(val));
        promise_.failure(std::errc::io_error); // dislike: wanna pass errno val here.
      }
    } else {
      DEBUG("Connecting to $0 failed. $1", inet_, strerror(val));
      promise_.failure(std::errc::io_error); // dislike: wanna pass errno val here.
    }
    delete this;
  }
};

template<>
std::string StringUtil::toString(InetConnectState* obj) {
  return StringUtil::format("InetConnectState[$0]", (void*) obj);
}

Future<RefPtr<EndPoint>> InetEndPoint::connectAsync(const InetAddress& inet,
                                                    Duration connectTimeout,
                                                    Duration readTimeout,
                                                    Duration writeTimeout,
                                                    Executor* executor) {
  int fd = socket(inet.family(), SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0)
    RAISE_ERRNO(errno);

  Promise<RefPtr<EndPoint>> promise;
  RefPtr<InetEndPoint> ep;

  try {
    TRACE("connectAsync: to $0", inet);
    ep = new InetEndPoint(fd, inet.family(), readTimeout, writeTimeout,
                          executor);
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
            promise.failure(std::errc::io_error);
            return promise.future();
          } else {
            TRACE("connectAsync: backgrounding");
            executor->executeOnWritable(fd,
                std::bind(&InetConnectState::onConnectComplete,
                          new InetConnectState(inet, ep, promise)));
            return promise.future();
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
                std::bind(&InetConnectState::onConnectComplete,
                          new InetConnectState(inet, std::move(ep),
                                               promise)));
            return promise.future();
          }
        }
        break;
      }
      default: {
        RAISE(NotImplementedError);
      }
    }

    TRACE("connectAsync: connected instantly");
    promise.success(ep.as<EndPoint>());
    return promise.future();
  } catch (...) {
    FileUtil::close(fd);
    throw;
  }
}

void InetEndPoint::connectAsync(
    const InetAddress& inet,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor,
    std::function<void(RefPtr<EndPoint>)> onSuccess,
    std::function<void(const std::error_code& ec)> onError) {
  Future<RefPtr<EndPoint>> f = connectAsync(
      inet, connectTimeout, readTimeout, writeTimeout, executor);

  f.onSuccess([onSuccess] (const RefPtr<EndPoint>& x) {
      onSuccess(const_cast<RefPtr<EndPoint>&>(x));
  });
  f.onFailure(onError);
}

RefPtr<EndPoint> InetEndPoint::connect(
    const InetAddress& inet,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor) {
  Future<RefPtr<EndPoint>> f = connectAsync(
      inet, connectTimeout, readTimeout, writeTimeout, executor);
  f.wait();

  RefPtr<EndPoint> ep = f.get();

  ep.as<InetEndPoint>()->setBlocking(true);

  return ep;
}

template<>
std::string StringUtil::toString(InetEndPoint* ep) {
  return StringUtil::format("$0", ep->remoteAddress());
}

} // namespace xzero
