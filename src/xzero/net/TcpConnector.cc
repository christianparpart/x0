// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/EventLoop.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpConnection.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/BufferUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

#include <algorithm>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#if defined(XZERO_OS_WINDOWS)
#include <io.h>
#include <WinSock2.h>
#endif

#if defined(XZERO_OS_UNIX)
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#endif

#if defined(SD_FOUND)
#include <systemd/sd-daemon.h>
#endif

#if !defined(SO_REUSEPORT)
#define SO_REUSEPORT 15
#endif

namespace xzero {

TcpConnector::TcpConnector(const std::string& name, Executor* executor,
                           ExecutorSelector clientExecutorSelector,
                           Duration readTimeout, Duration writeTimeout,
                           Duration tcpFinTimeout)
    : name_(name),
      executor_(executor),
      connectionFactories_(),
      defaultConnectionFactory_(),
      io_(),
      selectClientExecutor_(clientExecutorSelector
                                ? clientExecutorSelector
                                : [executor]() { return executor; }),
      address_(),
      connectedEndPoints_(),
      mutex_(),
      socket_(Socket::InvalidSocket),
      typeMask_(0),
      flags_(0),
      blocking_(true),
      backlog_(128),
      multiAcceptCount_(1),
      deferAccept_(false),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      tcpFinTimeout_(tcpFinTimeout),
      isStarted_(false),
      inDestructor_(false) {
#if defined(PLATFORM_WSL)
  if (tcpFinTimeout_ != Duration::Zero) {
    logWarning(
        "This software is running on WSL which doesn't support setting "
        "TCP_FIN timeout (TCP_LINGER2) yet. Ignoring.");
    tcpFinTimeout_ = Duration::Zero;
  }
#endif
}

TcpConnector::TcpConnector(const std::string& name,
                           Executor* executor,
                           ExecutorSelector clientExecutorSelector,
                           Duration readTimeout,
                           Duration writeTimeout,
                           Duration tcpFinTimeout,
                           const IPAddress& ipaddress, int port, int backlog,
                           bool reuseAddr, bool reusePort)
    : TcpConnector(name, executor, clientExecutorSelector,
                   readTimeout, writeTimeout, tcpFinTimeout) {

  open(ipaddress, port, backlog, reuseAddr, reusePort);
}

const std::string& TcpConnector::name() const {
  return name_;
}

void TcpConnector::open(const IPAddress& ipaddress, int port, int backlog,
                         bool reuseAddr, bool reusePort) {
  if (isOpen())
    logFatal("TcpConnector already open.");

  socket_ = std::move(Socket::make_tcp_ip(true, (Socket::AddressFamily) ipaddress.family()));

  setBacklog(backlog);

  if (reusePort)
    setReusePort(reusePort);

  if (reuseAddr)
    setReuseAddr(reuseAddr);

  bind(ipaddress, port);
}

void TcpConnector::bind(const IPAddress& ipaddr, int port) {
  char sa[sizeof(sockaddr_in6)];
  socklen_t salen = ipaddr.size();

  switch (ipaddr.family()) {
    case IPAddress::Family::V4:
      salen = sizeof(sockaddr_in);
      ((sockaddr_in*)sa)->sin_port = htons(port);
      ((sockaddr_in*)sa)->sin_family = AF_INET;
      memcpy(&((sockaddr_in*)sa)->sin_addr, ipaddr.data(), ipaddr.size());
      break;
    case IPAddress::Family::V6:
      salen = sizeof(sockaddr_in6);
      ((sockaddr_in6*)sa)->sin6_port = htons(port);
      ((sockaddr_in6*)sa)->sin6_family = AF_INET6;
      memcpy(&((sockaddr_in6*)sa)->sin6_addr, ipaddr.data(), ipaddr.size());
      break;
    default:
      RAISE_ERRNO(EINVAL);
  }

  int rv = ::bind(socket_, (sockaddr*)sa, salen);
  if (rv < 0)
    RAISE_ERRNO(errno);

  if (port != 0) {
    address_ = InetAddress{ipaddr, port};
  } else {
    address_ = InetAddress{ipaddr, socket_.getLocalPort()};
  }
}

static std::string readFile(const std::string& filename) {
  std::ifstream f(filename);
  std::stringstream sstr;
  sstr << f.rdbuf();
  return std::move(sstr.str());
}

void TcpConnector::listen(int backlog) {
  int somaxconn = SOMAXCONN;

#if defined(XZERO_OS_LINUX)
  somaxconn = std::stoi(readFile("/proc/sys/net/core/somaxconn"));
#endif

  if (backlog > somaxconn) {
    throw std::runtime_error{fmt::format(
        "Listener {} configured with a backlog higher than the system"
        " permits ({} > {})."
#if defined(XZERO_OS_LINUX)
        " See /proc/sys/net/core/somaxconn for your system limits."
#endif
        ,
        address_,
        backlog,
        somaxconn)};
  }

  if (backlog <= 0) {
    backlog = somaxconn;
  }

  int rv = ::listen(socket_, backlog);
  if (rv < 0)
    RAISE_ERRNO(errno);
}

bool TcpConnector::isOpen() const noexcept {
  return socket_ >= 0;
}

TcpConnector::~TcpConnector() {
  inDestructor_ = true;

  if (isStarted()) {
    stop();
  }
}

int TcpConnector::handle() const noexcept {
  return socket_;
}

size_t TcpConnector::backlog() const noexcept {
  return backlog_;
}

void TcpConnector::setBacklog(size_t value) {
  if (isStarted()) {
    throw std::logic_error{"TcpConnector.setBacklog cannot be invoked when already opened."};
  }

  backlog_ = value;
}

void TcpConnector::setBlocking(bool enable) {
  socket_.setBlocking(enable);
  blocking_ = enable;

#if defined(XZERO_OS_WINDOWS)
  /* */
#elif defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
  if (enable) {
    typeMask_ &= ~SOCK_NONBLOCK;
  } else {
    typeMask_ |= SOCK_NONBLOCK;
  }
#else
  if (enable) {
    flags_ &= ~O_NONBLOCK;
  } else {
    flags_ |= O_NONBLOCK;
  }
#endif
}

void TcpConnector::setCloseOnExec(bool enable) {
#if defined(XZERO_OS_WINDOWS)
  /* */
#else
  unsigned flags = enable ? fcntl(socket_, F_GETFD) | FD_CLOEXEC
                          : fcntl(socket_, F_GETFD) & ~FD_CLOEXEC;

  if (fcntl(socket_, F_SETFD, flags) < 0) {
    RAISE_ERRNO(errno);
  }

#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
  if (enable) {
    typeMask_ |= SOCK_CLOEXEC;
  } else {
    typeMask_ &= ~SOCK_CLOEXEC;
  }
#else
  if (enable) {
    flags_ |= O_CLOEXEC;
  } else {
    flags_ &= ~O_CLOEXEC;
  }
#endif
#endif
}

bool TcpConnector::deferAccept() const {
  return deferAccept_;
}

void TcpConnector::setDeferAccept(bool enable) {
#if defined(TCP_DEFER_ACCEPT)
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_TCP, TCP_DEFER_ACCEPT, &rc, sizeof(rc)) < 0) {
    switch (errno) {
      case ENOPROTOOPT:
      case ENOTSUP:
#if defined(EOPNOTSUPP) && (EOPNOTSUPP != ENOTSUP)
      case EOPNOTSUPP:
#endif
        logWarning("TcpConnector: setDeferAccept({}) failed with {} ({}). Ignoring",
                   enable ? "true" : "false",
                   strerror(errno),
                   errno);
        return;
      default:
        RAISE_ERRNO(errno);
    }
  }
  deferAccept_ = enable;
#elif defined(SO_ACCEPTFILTER)
  // XXX this compiles on FreeBSD but not on OSX (why don't they support it?)
  accept_filter_arg afa;
  strcpy(afa.af_name, "dataready");
  afa.af_arg[0] = 0;

  if (setsockopt(serverfd, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa)) < 0) {
    RAISE_ERRNO(errno);
  }
  deferAccept_ = enable;
#else
  if (enable) {
    logWarning("TcpConnector: Ignoring setting TCP_DEFER_ACCEPT. Not supported.");
  } else {
    deferAccept_ = enable;
  }
#endif
}

void TcpConnector::setQuickAck(bool enable) {
#if defined(TCP_QUICKACK)
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_TCP, TCP_QUICKACK, &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
#else
  // ignore
#endif
}

void TcpConnector::setReusePort(bool enable) {
  int rc = enable ? 1 : 0; // XXX windows requires cast to (const char*)
  if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, (const char*) &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
}

bool TcpConnector::isReusePortSupported() {
  Socket s = Socket::make_tcp_ip(false, Socket::AddressFamily::V4);
  if (!s.valid())
    return false;

  int rc = 1;
  return ::setsockopt(s.native(), SOL_SOCKET, SO_REUSEPORT, (const char*) &rc, sizeof(rc)) == 0;
}

bool TcpConnector::isDeferAcceptSupported() {
#if defined(TCP_DEFER_ACCEPT)
  Socket s = Socket::make_tcp_ip(false, Socket::AddressFamily::V4);
  if (!s.valid())
    return false;

  int rc = 1;
  return ::setsockopt(s.native(), SOL_TCP, TCP_DEFER_ACCEPT, &rc, sizeof(rc)) == 0;
#else
  return false;
#endif
}

bool TcpConnector::reuseAddr() const {
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (char*) &optval, &optlen) == 0
             ? optval != 0
             : false;
}

void TcpConnector::setReuseAddr(bool enable) {
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (char*) &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
}

size_t TcpConnector::multiAcceptCount() const noexcept {
  return multiAcceptCount_;
}

void TcpConnector::setMultiAcceptCount(size_t value) noexcept {
  multiAcceptCount_ = value;
}

void TcpConnector::setReadTimeout(Duration value) {
  readTimeout_ = value;
}

void TcpConnector::setWriteTimeout(Duration value) {
  writeTimeout_ = value;
}

void TcpConnector::setTcpFinTimeout(Duration value) {
  tcpFinTimeout_ = value;
}

void TcpConnector::start() {
  if (!isOpen()) {
    throw std::logic_error{"TcpConnector is already started."};
  }

  if (isStarted()) {
    return;
  }

  listen(backlog_);

  isStarted_ = true;

  if (deferAccept_) {
    setDeferAccept(true);
  }

  notifyOnEvent();
}

void TcpConnector::notifyOnEvent() {
  io_ = executor()->executeOnReadable(
      socket_,
      std::bind(&TcpConnector::onConnect, this));
}

bool TcpConnector::isStarted() const noexcept {
  return isStarted_;
}

void TcpConnector::stop() {
  if (io_)
    io_->cancel();

  if (isOpen())
    socket_.close();

  isStarted_ = false;
}

void TcpConnector::onConnect() {
  try {
    for (size_t i = 0; i < multiAcceptCount_; i++) {
      std::optional<Socket> clientSocket = acceptOne();
      if (!clientSocket)
        break;

      Executor* clientExecutor = selectClientExecutor_();
      std::shared_ptr<TcpEndPoint> ep = createEndPoint(std::move(*clientSocket), clientExecutor);
      {
        std::lock_guard<std::mutex> _lk(mutex_);
        connectedEndPoints_.push_back(ep);
      }

      clientExecutor->execute(
          std::bind(&TcpConnector::onEndPointCreated, this, ep));
    }
  } catch (const std::exception& e) {
    logError("Failed accepting client connection. {}", e.what());
  }

  if (isStarted()) {
    notifyOnEvent();
  }
}

std::optional<Socket> TcpConnector::acceptOne() {
#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
  bool flagged = true;
  FileDescriptor cfd = ::accept4(socket_, nullptr, 0, typeMask_);
  if (cfd < 0 && errno == ENOSYS) {
    cfd = ::accept(socket_, nullptr, 0);
    flagged = false;
  }
#else
  bool flagged = false;
  FileDescriptor cfd = ::accept(socket_, nullptr, 0);
#endif

  if (cfd < 0) {
    switch (errno) {
    case EINTR:
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
      return std::nullopt;
    default:
      RAISE_ERRNO(errno);
    }
  }

#if defined(TCP_LINGER2)
  if (int waitTime = tcpFinTimeout_.seconds(); waitTime != 0) {
    if (setsockopt(cfd, SOL_TCP, TCP_LINGER2, (const char*) &waitTime, sizeof(waitTime)) < 0) {
      RAISE_ERRNO(errno);
    }
  }
#endif

#if defined(XZERO_OS_UNIX)
  if (!flagged && flags_ && fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL) | flags_) < 0)
    RAISE_ERRNO(errno);

  return Socket::make_socket(addressFamily(), std::move(cfd));
#elif defined(XZERO_OS_WINDOWS)
  Socket cs = Socket::make_socket(addressFamily(), std::move(cfd));
  if (!blocking_) {
    cs.setBlocking(blocking_);
  }
  // XXX O_CLOEXEC
  return cs;
#endif
}

std::shared_ptr<TcpEndPoint> TcpConnector::createEndPoint(Socket&& cfd, Executor* executor) {
  return std::make_shared<TcpEndPoint>(std::move(cfd),
      readTimeout_, writeTimeout_, executor_,
      std::bind(&TcpConnector::onEndPointClosed, this, std::placeholders::_1));
}

void TcpConnector::onEndPointCreated(std::shared_ptr<TcpEndPoint> endpoint) {
  if (connectionFactoryCount() > 1) {
    endpoint->startDetectProtocol(
        deferAccept(),
        std::bind(&TcpConnector::createConnection, this,
                  std::placeholders::_1,
                  std::placeholders::_2));
  } else {
    std::unique_ptr<TcpConnection> c =
        defaultConnectionFactory()(this, endpoint.get());
    endpoint->setConnection(std::move(c));
    endpoint->connection()->onOpen(deferAccept());
  }
}

std::list<std::shared_ptr<TcpEndPoint>> TcpConnector::connectedEndPoints() {
  std::list<std::shared_ptr<TcpEndPoint>> result;
  std::lock_guard<std::mutex> _lk(mutex_);
  for (const std::shared_ptr<TcpEndPoint>& ep : connectedEndPoints_) {
    result.push_back(ep);
  }
  return result;
}

void TcpConnector::onEndPointClosed(TcpEndPoint* endpoint) {
  if (inDestructor_)
    return;

  assert(endpoint != nullptr);

  // XXX: e.g. SSL doesn't have a connection in case the handshake failed
  // assert(endpoint->connection() != nullptr);

  std::lock_guard<std::mutex> _lk(mutex_);

  auto i = std::find_if(connectedEndPoints_.begin(), connectedEndPoints_.end(),
      [endpoint](auto obj) { return obj.get() == endpoint; });

  assert(i != connectedEndPoints_.end());

  if (i != connectedEndPoints_.end()) {
    connectedEndPoints_.erase(i);
  }
}

void TcpConnector::addConnectionFactory(const std::string& protocolName,
                                        ConnectionFactory factory) {
  assert(protocolName != "");
  assert(!!factory);

  connectionFactories_[protocolName] = factory;

  if (connectionFactories_.size() == 1) {
    defaultConnectionFactory_ = protocolName;
  }
}

void TcpConnector::createConnection(const std::string& protocolName,
                                    TcpEndPoint* endpoint) {
  auto factory = connectionFactory(protocolName);
  std::unique_ptr<TcpConnection> c = factory
      ? factory(this, endpoint)
      : defaultConnectionFactory()(this, endpoint);
  endpoint->setConnection(std::move(c));
  endpoint->connection()->onOpen(endpoint->readBufferSize() > 0);
}

TcpConnector::ConnectionFactory TcpConnector::connectionFactory(
    const std::string& protocolName) const {
  auto i = connectionFactories_.find(protocolName);
  if (i != connectionFactories_.end()) {
    return i->second;
  }
  return nullptr;
}

std::vector<std::string> TcpConnector::connectionFactories() const {
  std::vector<std::string> result;
  result.reserve(connectionFactories_.size());

  for (auto& entry: connectionFactories_) {
    result.push_back(entry.first);
  }

  return result;
}

size_t TcpConnector::connectionFactoryCount() const {
  return connectionFactories_.size();
}

void TcpConnector::setDefaultConnectionFactory(const std::string& protocolName) {
  auto i = connectionFactories_.find(protocolName);
  if (i == connectionFactories_.end())
    throw std::invalid_argument{"protocolName"};

  defaultConnectionFactory_ = protocolName;
}

TcpConnector::ConnectionFactory TcpConnector::defaultConnectionFactory() const {
  auto i = connectionFactories_.find(defaultConnectionFactory_);
  if (i == connectionFactories_.end())
    throw std::logic_error{"No connection factories available in TcpConnector yet."};

  return i->second;
}

void TcpConnector::loadConnectionFactorySelector(const std::string& protocolName,
                                                 Buffer* sink) {
  auto i = connectionFactories_.find(defaultConnectionFactory_);
  if (i == connectionFactories_.end())
    throw std::invalid_argument{"protocolName"};

  sink->push_back((char) MagicProtocolSwitchByte);
  BinaryWriter(BufferUtil::writer(sink)).writeString(protocolName);
}

std::string TcpConnector::toString() const {
  return fmt::format("TcpConnector({})<{}>", name(), address_);
}

}  // namespace xzero
