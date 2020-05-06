// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/net/TcpConnection.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>

#include <stdexcept>
#include <fcntl.h>
#include <errno.h>

#if defined(HAVE_SYS_SENDFILE_H)
#include <sys/sendfile.h>
#endif

#if defined(XZERO_OS_UNIX)
#include <netinet/tcp.h>
#include <unistd.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#include <WinSock2.h>
#include <MSWSock.h>
#endif

namespace xzero {

TcpEndPoint::TcpEndPoint(Duration readTimeout,
                         Duration writeTimeout,
                         Executor* executor,
                         std::function<void(TcpEndPoint*)> onEndPointClosed)
    : io_(),
      executor_(executor),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      inputBuffer_(),
      inputOffset_(0),
      socket_{Socket::InvalidSocket},
      isCorking_(false),
      onEndPointClosed_(onEndPointClosed),
      connection_() {
}

TcpEndPoint::TcpEndPoint(Socket&& socket,
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
      socket_(std::move(socket)),
      isCorking_(false),
      onEndPointClosed_(onEndPointClosed),
      connection_() {
}

void TcpEndPoint::onTimeout() {
  if (connection()) {
    if (connection()->onReadTimeout()) {
      close();
    }
  }
}

TcpEndPoint::~TcpEndPoint() {
  if (isOpen()) {
    close();
  }
}

std::optional<InetAddress> TcpEndPoint::remoteAddress() const {
  Result<InetAddress> addr = socket_.getRemoteAddress();
  if (addr.isSuccess())
    return *addr;
  else {
    logError("TcpEndPoint: remoteAddress: ({}) {}",
        addr.error().category().name(),
        addr.error().message().c_str());
    return std::nullopt;
  }
}

std::optional<InetAddress> TcpEndPoint::localAddress() const {
  Result<InetAddress> addr = socket_.getLocalAddress();
  if (addr.isSuccess())
    return *addr;
  else {
    logError("TcpEndPoint: localAddress: ({}) {}",
        addr.error().category().name(),
        addr.error().message().c_str());
    return std::nullopt;
  }
}

bool TcpEndPoint::isOpen() const noexcept {
  return socket_.valid();
}

void TcpEndPoint::close() {
  if (isOpen()) {
    socket_.close();

    if (onEndPointClosed_) {
      onEndPointClosed_(this);
    }
  }
}

void TcpEndPoint::setConnection(std::unique_ptr<TcpConnection>&& c) {
  connection_ = std::move(c);
}

void TcpEndPoint::setBlocking(bool enable) {
  socket_.setBlocking(enable);
}

void TcpEndPoint::setCorking(bool enable) {
  if (isCorking_ != enable) {
#if defined(TCP_CORK)
    int flag = enable ? 1 : 0;
    if (setsockopt(socket_, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) < 0)
      RAISE_ERRNO(errno);
#endif
    isCorking_ = enable;
  }
}

void TcpEndPoint::setTcpNoDelay(bool enable) {
  int flag = enable ? 1 : 0;
  if (setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, (const char*) &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
}

std::string TcpEndPoint::toString() const {
  return fmt::format("TcpEndPoint({})", socket_.getRemoteAddress());
}

void TcpEndPoint::startDetectProtocol(bool dataReady, ProtocolCallback createConnection) {
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
    count = std::min(count, inputBuffer_.size() - inputOffset_);
    result->push_back(inputBuffer_.ref(inputOffset_, count));
    inputOffset_ += count;
    if (inputOffset_ == inputBuffer_.size()) {
      inputBuffer_.clear();
      inputOffset_ = 0;
    }
    return count;
  }

#if defined(XZERO_OS_WINDOWS)
  int n = recv(handle(), result->end(), count, 0);
  if (n == SOCKET_ERROR) {
    if (int ec = WSAGetLastError(); ec != WSAEWOULDBLOCK) {
      RAISE_WSA_ERROR(WSAGetLastError());
    }
  } else if (n > 0) {
    result->resize(result->size() + n);
  }
  return n;
#else
  ssize_t n = ::read(handle(), result->end(), count);
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
#endif
}

size_t TcpEndPoint::write(const BufferRef& source) {
#if defined(XZERO_OS_WINDOWS)
  int rv = ::send(handle(), source.data(), source.size(), 0);
  if (rv == SOCKET_ERROR)
    RAISE_WSA_ERROR(WSAGetLastError());
#else
  ssize_t rv = ::write(handle(), source.data(), source.size());
  if (rv < 0)
    RAISE_ERRNO(errno);
#endif

  // EOF exception?

  return rv;
}

size_t TcpEndPoint::write(const FileView& source) {
#if defined(XZERO_OS_WINDOWS)
  TransmitFile(handle(), source.handle(), source.size(), 0, nullptr, nullptr, 0); // TODO: proper non-blocking call & error handling
  return source.size();
#elif defined(__APPLE__)
  off_t len = source.size();
  int rv = ::sendfile(source.handle(), handle(), source.offset(), &len, nullptr, 0);
  if (rv < 0)
    RAISE_ERRNO(errno);

  return len;
#else
  off_t offset = source.offset();
  ssize_t rv = ::sendfile(handle(), source.handle(), &offset, source.size());
  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
#endif
}

void TcpEndPoint::wantRead() {
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
  if (!io_) {
    io_ = executor_->executeOnWritable(
        handle(),
        std::bind(&TcpEndPoint::flushable, this),
        writeTimeout(),
        std::bind(&TcpEndPoint::onTimeout, this));
  }
}

void TcpEndPoint::flushable() {
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

void TcpEndPoint::connect(const InetAddress& address,
                          Duration connectTimeout,
                          std::function<void()> onConnected,
                          std::function<void(std::error_code ec)> onFailure) {
  socket_ = Socket::make_tcp_ip(true, address.family());
  std::error_code ec = socket_.connect(address);
  if (!ec) {
    onConnected();
  } else if (ec == std::errc::operation_in_progress) {
    executor_->executeOnWritable(socket_,
        [=]() { onConnectComplete(address, onConnected, onFailure); },
        connectTimeout,
        [=]() { onFailure(std::make_error_code(std::errc::timed_out)); });
  } else {
    onFailure(ec);
  }
}

void TcpEndPoint::onConnectComplete(InetAddress address,
                                    std::function<void()> onConnected,
                                    std::function<void(std::error_code ec)> onFailure) {
  int val = 0;
  socklen_t vlen = sizeof(val);
  if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, (char*) &val, &vlen) == 0) {
    if (val == 0) {
      onConnected();
    } else {
      const std::error_code ec = std::make_error_code(static_cast<std::errc>(val));
      logDebug("Connecting to {} failed. {}", address, ec.message());
      onFailure(ec);
    }
  } else {
    const std::error_code ec = std::make_error_code(static_cast<std::errc>(val));
    logDebug("Connecting to {} failed. {}", address, ec.message());
    onFailure(ec);
  }
}

} // namespace xzero
