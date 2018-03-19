// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/net/TcpConnection.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/io/FileUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <stdexcept>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace xzero {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("TcpEndPoint: " msg)
#define DEBUG(msg...) logDebug("TcpEndPoint: " msg)
#else
#define TRACE(msg...) do {} while (0)
#define DEBUG(msg...) do {} while (0)
#endif

TcpEndPoint::TcpEndPoint(FileDescriptor&& socket,
                         int addressFamily,
                         Duration readTimeout,
                         Duration writeTimeout,
                         Executor* executor,
                         std::function<void(TcpEndPoint*)> onEndPointClosed)
    : io_(),
      executor_(executor),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      inputBuffer_(),
      inputOffset_(0),
      handle_(std::move(socket)),
      addressFamily_(addressFamily),
      isCorking_(false),
      onEndPointClosed_(onEndPointClosed),
      connection_() {
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
    logError("TcpEndPoint: remoteAddress: ($0) $1",
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
    logError("TcpEndPoint: localAddress: ($0) $1",
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
    TRACE("close() fd=$0", handle_.get());

    if (onEndPointClosed_) {
      onEndPointClosed_(this);
    }

    handle_.close();
  } else {
    TRACE("close(fd=$0) invoked, but we're closed already", handle_.get());
  }
}

void TcpEndPoint::setConnection(std::unique_ptr<TcpConnection>&& c) {
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

void TcpEndPoint::startDetectProtocol(bool dataReady,
                                      ProtocolCallback createConnection) {
  inputBuffer_.reserve(256);

  if (dataReady) {
    onDetectProtocol(createConnection);
  } else {
    executor_->executeOnReadable(
        handle(),
        std::bind(&TcpEndPoint::onDetectProtocol, this, createConnection));
  }
}

void TcpEndPoint::onDetectProtocol(ProtocolCallback createConnection) {
  size_t n = read(&inputBuffer_);

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
    // create TcpConnection object for given endpoint
    TRACE("$0 protocol-switch not detected.", this);
    createConnection("", this);
  }

  if (connection_) {
    connection_->onOpen(true);
  } else {
    close();
  }
}

size_t TcpEndPoint::readahead(size_t maxBytes) {
  const size_t nprefilled = readBufferSize();
  if (nprefilled > 0)
    return nprefilled;

  inputBuffer_.reserve(maxBytes);
  return read(&inputBuffer_);
}

size_t TcpEndPoint::read(Buffer* sink) {
  int space = sink->capacity() - sink->size();
  if (space < 4 * 1024) {
    sink->reserve(sink->capacity() + 8 * 1024);
    space = sink->capacity() - sink->size();
  }

  return read(sink, space);
}

size_t TcpEndPoint::read(Buffer* result, size_t count) {
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

  ssize_t n = ::read(handle(), result->end(), count);
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

size_t TcpEndPoint::write(const BufferRef& source) {
  ssize_t rv = ::write(handle(), source.data(), source.size());

  TRACE("write($0 bytes) -> $1", source.size(), rv);

  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
}

size_t TcpEndPoint::write(const FileView& view) {
  return TcpUtil::sendfile(handle(), view);
}

void TcpEndPoint::wantRead() {
  TRACE("$0 wantRead()", this);
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
  std::shared_ptr<TcpEndPoint> _guard = shared_from_this();

  try {
    io_.reset();
    connection()->onReadable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  }
}

void TcpEndPoint::wantWrite() {
  TRACE("$0 wantWrite() $1", this, io_.get() ? "again" : "first time");

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
  std::shared_ptr<TcpEndPoint> _guard = shared_from_this();

  try {
    io_.reset();
    connection()->onWriteable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  }
}

Duration TcpEndPoint::readTimeout() const noexcept {
  return readTimeout_;
}

Duration TcpEndPoint::writeTimeout() const noexcept {
  return writeTimeout_;
}

class TcpConnectState {
 public:
  InetAddress address;
  int fd;
  Duration readTimeout;
  Duration writeTimeout;
  Executor* executor;
  Promise<std::shared_ptr<TcpEndPoint>> promise;

  void onTimeout() {
    TRACE("$0 onTimeout: connecting timed out", this);
    promise.failure(std::errc::timed_out);
    delete this;
  }

  void onConnectComplete() {
    int val = 0;
    socklen_t vlen = sizeof(val);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
      if (val == 0) {
        TRACE("$0 onConnectComplete: Connected.", this);
        promise.success(std::make_shared<TcpEndPoint>(FileDescriptor{fd},
                                                      address.family(),
                                                      readTimeout,
                                                      writeTimeout,
                                                      executor,
                                                      nullptr));
      } else {
        DEBUG("Connecting to $0 failed. $1", address, strerror(val));
        promise.failure(std::make_error_code(static_cast<std::errc>(val)));
      }
    } else {
      DEBUG("Connecting to $0 failed. $1", address, strerror(val));
      promise.failure(std::make_error_code(static_cast<std::errc>(val)));
    }
    delete this;
  }
};

Future<std::shared_ptr<TcpEndPoint>> TcpEndPoint::connect(const InetAddress& address,
                                                          Duration connectTimeout,
                                                          Duration readTimeout,
                                                          Duration writeTimeout,
                                                          Executor* executor) {
  int flags = 0;
#if defined(SOCK_CLOEXEC)
  flags |= SOCK_CLOEXEC;
#endif
#if defined(SOCK_NONBLOCK)
  flags |= SOCK_NONBLOCK;
#endif

  Promise<std::shared_ptr<TcpEndPoint>> promise;

  int fd = socket(address.family(), SOCK_STREAM | flags, IPPROTO_TCP);
  if (fd < 0) {
    promise.failure(std::make_error_code(static_cast<std::errc>(errno)));
    return promise.future();
  }

#if !defined(SOCK_NONBLOCK)
  FileUtil::setBlocking(fd, false);
#endif

  std::error_code ec = TcpUtil::connect(fd, address);
  if (!ec) {
    TRACE("connect: connected instantly");
    promise.success(std::make_shared<TcpEndPoint>(FileDescriptor{fd},
                                                  address.family(),
                                                  readTimeout,
                                                  writeTimeout,
                                                  executor,
                                                  nullptr));
  } else if (ec == std::errc::operation_in_progress) {
    TRACE("connect: backgrounding");
    auto state = new TcpConnectState{address, fd, readTimeout, writeTimeout,
                                     executor, promise};
    executor->executeOnWritable(fd,
        std::bind(&TcpConnectState::onConnectComplete, state),
        connectTimeout,
        std::bind(&TcpConnectState::onTimeout, state));
  } else {
    TRACE("connect: connect() error. $0", strerror(errno));
    promise.failure(std::make_error_code(static_cast<std::errc>(errno)));
  }
  return promise.future();
}

} // namespace xzero
