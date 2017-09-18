// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/EventLoop.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/net/Connection.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileUtil.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/BufferUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <algorithm>
#include <mutex>
#include <system_error>

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#if defined(SD_FOUND)
#include <systemd/sd-daemon.h>
#endif

#if !defined(SO_REUSEPORT)
#define SO_REUSEPORT 15
#endif

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("net.TcpConnector", msg)
#else
#define TRACE(msg...) do {} while (0)
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
      bindAddress_(),
      port_(-1),
      connectedEndPoints_(),
      mutex_(),
      socket_(-1),
      addressFamily_(IPAddress::V4),
      typeMask_(0),
      flags_(0),
      blocking_(true),
      backlog_(128),
      multiAcceptCount_(1),
      deferAccept_(false),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      tcpFinTimeout_(tcpFinTimeout),
      isStarted_(false) {
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
    RAISE_STATUS(IllegalStateError);

  socket_ = ::socket(ipaddress.family(), SOCK_STREAM, 0);

  TRACE("open: ip=$0, port=$1, backlog=$2, reuseAddr=$3, reusePort=$4",
      ipaddress, port, backlog, reuseAddr, reusePort);

  if (socket_ < 0)
    RAISE_ERRNO(errno);

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
    case IPAddress::V4:
      salen = sizeof(sockaddr_in);
      ((sockaddr_in*)sa)->sin_port = htons(port);
      ((sockaddr_in*)sa)->sin_family = AF_INET;
      memcpy(&((sockaddr_in*)sa)->sin_addr, ipaddr.data(), ipaddr.size());
      break;
    case IPAddress::V6:
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

  addressFamily_ = ipaddr.family();
  bindAddress_ = ipaddr;
  if (port != 0) {
    port_ = port;
  } else {
    port_ = TcpUtil::getLocalPort(socket_, addressFamily_);
  }
}

void TcpConnector::listen(int backlog) {
  int somaxconn = SOMAXCONN;

#if defined(XZERO_OS_LINUX)
  somaxconn = FileUtil::read("/proc/sys/net/core/somaxconn").toInt();
#endif

  if (backlog > somaxconn) {
    RAISE(
        RuntimeError,
        "Listener %s:%d configured with a backlog higher than the system"
        " permits (%ld > %ld)."
#if defined(XZERO_OS_LINUX)
        " See /proc/sys/net/core/somaxconn for your system limits."
#endif
        ,
        bindAddress_.str().c_str(),
        port_,
        backlog,
        somaxconn);
  }

  if (backlog <= 0) {
    backlog = somaxconn;
  }

  int rv = ::listen(socket_, backlog);
  if (rv < 0)
    RAISE_ERRNO(errno);
}

bool TcpConnector::isOpen() const XZERO_NOEXCEPT {
  return socket_ >= 0;
}

TcpConnector::~TcpConnector() {
  TRACE("~TcpConnector");
  if (isStarted()) {
    stop();
  }
}

int TcpConnector::handle() const XZERO_NOEXCEPT {
  return socket_;
}

void TcpConnector::setSocket(FileDescriptor&& socket) {
  socket_ = std::move(socket);
}

size_t TcpConnector::backlog() const XZERO_NOEXCEPT {
  return backlog_;
}

void TcpConnector::setBacklog(size_t value) {
  if (isStarted()) {
    RAISE_STATUS(IllegalStateError);
  }

  backlog_ = value;
}

bool TcpConnector::isBlocking() const {
  return !(fcntl(socket_, F_GETFL) & O_NONBLOCK);
}

void TcpConnector::setBlocking(bool enable) {
  unsigned flags = enable ? fcntl(socket_, F_GETFL) & ~O_NONBLOCK
                          : fcntl(socket_, F_GETFL) | O_NONBLOCK;

  if (fcntl(socket_, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }

#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
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
  blocking_ = enable;
}

bool TcpConnector::closeOnExec() const {
  return fcntl(socket_, F_GETFD) & FD_CLOEXEC;
}

void TcpConnector::setCloseOnExec(bool enable) {
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
        logWarning("TcpConnector", "setDeferAccept($0) failed with $1 ($2). Ignoring",
            enable ? "true" : "false", strerror(errno), errno);
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
    logWarning("TcpConnector",
               "Ignoring setting TCP_DEFER_ACCEPT. Not supported.");
  } else {
    deferAccept_ = enable;
  }
#endif
}

bool TcpConnector::quickAck() const {
#if defined(TCP_QUICKACK)
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_TCP, TCP_QUICKACK, &optval, &optlen) == 0
             ? optval != 0
             : false;
#else
  return false;
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

bool TcpConnector::reusePort() const {
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &optval, &optlen) == 0
             ? optval != 0
             : false;
}

void TcpConnector::setReusePort(bool enable) {
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
}

bool TcpConnector::isReusePortSupported() {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;

  int rc = 1;
  bool res = ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &rc, sizeof(rc)) == 0;

  FileUtil::close(fd);
  return res;
}

bool TcpConnector::isDeferAcceptSupported() {
#if defined(TCP_DEFER_ACCEPT)
  FileDescriptor fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;

  int rc = 1;
  bool res = ::setsockopt(fd, SOL_TCP, TCP_DEFER_ACCEPT, &rc, sizeof(rc)) == 0;

  return res;
#else
  return false;
#endif
}

bool TcpConnector::reuseAddr() const {
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  return ::getsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen) == 0
             ? optval != 0
             : false;
}

void TcpConnector::setReuseAddr(bool enable) {
  int rc = enable ? 1 : 0;
  if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof(rc)) < 0) {
    RAISE_ERRNO(errno);
  }
}

size_t TcpConnector::multiAcceptCount() const XZERO_NOEXCEPT {
  return multiAcceptCount_;
}

void TcpConnector::setMultiAcceptCount(size_t value) XZERO_NOEXCEPT {
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
  TRACE("start: ip=$0, port=$1", bindAddress_, port_);
  if (!isOpen()) {
    RAISE_STATUS(IllegalStateError);
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
      handle(),
      std::bind(&TcpConnector::onConnect, this));
}

bool TcpConnector::isStarted() const XZERO_NOEXCEPT {
  return isStarted_;
}

void TcpConnector::stop() {
  TRACE("stop: $0", this);

  if (io_)
    io_->cancel();

  if (isOpen())
    FileUtil::close(socket_.release());

  isStarted_ = false;
}

void TcpConnector::onConnect() {
  for (size_t i = 0; i < multiAcceptCount_; i++) {
    int cfd = acceptOne();
    if (cfd < 0)
      break;

    TRACE("onConnect: fd=$0", cfd);

    Executor* clientExecutor = selectClientExecutor_();
    RefPtr<TcpEndPoint> ep = createEndPoint(cfd, clientExecutor);
    {
      std::lock_guard<std::mutex> _lk(mutex_);
      connectedEndPoints_.push_back(ep);
    }

    clientExecutor->execute(
        std::bind(&TcpConnector::onEndPointCreated, this, ep));
  }

  if (isStarted()) {
    notifyOnEvent();
  }
}

int TcpConnector::acceptOne() {
#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
  bool flagged = true;
  int cfd = ::accept4(socket_, nullptr, 0, typeMask_);
  if (cfd < 0 && errno == ENOSYS) {
    cfd = ::accept(socket_, nullptr, 0);
    flagged = false;
  }
#else
  bool flagged = false;
  int cfd = ::accept(socket_, nullptr, 0);
#endif

  if (cfd < 0) {
    switch (errno) {
      case EINTR:
      case EAGAIN:
#if EAGAIN != EWOULDBLOCK
      case EWOULDBLOCK:
#endif
        return -1;
      default:
        RAISE_ERRNO(errno);
    }
  }

  if (!flagged && flags_ &&
        fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL) | flags_) < 0) {
    FileUtil::close(cfd);
    RAISE_ERRNO(errno);
  }

#if defined(TCP_LINGER2)
  if (tcpFinTimeout_ != Duration::Zero) {
    int waitTime = tcpFinTimeout_.seconds();
    int rv = setsockopt(cfd, SOL_TCP, TCP_LINGER2, &waitTime, sizeof(waitTime));
    if (rv < 0) {
      FileUtil::close(cfd);
      RAISE_ERRNO(errno);
    }
  }
#endif

  return cfd;
}

RefPtr<TcpEndPoint> TcpConnector::createEndPoint(int cfd, Executor* executor) {
  return make_ref<TcpEndPoint>(cfd, addressFamily(),
      readTimeout_, writeTimeout_, executor_,
      std::bind(&TcpConnector::onEndPointClosed, this, std::placeholders::_1));
}

void TcpConnector::onEndPointCreated(RefPtr<TcpEndPoint> endpoint) {
  if (connectionFactoryCount() > 1) {
    endpoint->startDetectProtocol(
        deferAccept(),
        std::bind(&TcpConnector::createConnection, this,
                  std::placeholders::_1,
                  std::placeholders::_2));
  } else {
    defaultConnectionFactory()(this, endpoint.get());
    endpoint->connection()->onOpen(deferAccept());
  }
}

std::list<RefPtr<TcpEndPoint>> TcpConnector::connectedEndPoints() {
  std::list<RefPtr<TcpEndPoint>> result;
  std::lock_guard<std::mutex> _lk(mutex_);
  for (const RefPtr<TcpEndPoint>& ep : connectedEndPoints_) {
    result.push_back(ep);
  }
  return result;
}

void TcpConnector::onEndPointClosed(TcpEndPoint* endpoint) {
  assert(endpoint != nullptr);

  // XXX: e.g. SSL doesn't have a connection in case the handshake failed
  // assert(endpoint->connection() != nullptr);

  std::lock_guard<std::mutex> _lk(mutex_);

  auto i = std::find(connectedEndPoints_.begin(), connectedEndPoints_.end(),
                     endpoint);

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
  TRACE("createConnection: \"$0\"", protocolName);
  auto factory = connectionFactory(protocolName);
  if (factory) {
    factory(this, endpoint);
  } else {
    defaultConnectionFactory()(this, endpoint);
  }
  endpoint->connection()->onOpen(endpoint->prefilled() > 0);
}

TcpConnector::ConnectionFactory TcpConnector::connectionFactory(
    const std::string& protocolName) const {
  auto i = connectionFactories_.find(protocolName);
  if (i != connectionFactories_.end()) {
    return i->second;
  }
  return nullptr;
}

std::list<std::string> TcpConnector::connectionFactories() const {
  std::list<std::string> result;
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
    throw std::runtime_error("Invalid argument.");

  defaultConnectionFactory_ = protocolName;
}

TcpConnector::ConnectionFactory TcpConnector::defaultConnectionFactory() const {
  auto i = connectionFactories_.find(defaultConnectionFactory_);
  if (i == connectionFactories_.end())
    RAISE_STATUS(InternalError);

  return i->second;
}

void TcpConnector::loadConnectionFactorySelector(const std::string& protocolName,
                                                  Buffer* sink) {
  auto i = connectionFactories_.find(defaultConnectionFactory_);
  if (i == connectionFactories_.end())
    RAISE(InvalidArgumentError, "Invalid protocol name.");

  sink->push_back((char) MagicProtocolSwitchByte);
  BinaryWriter(BufferUtil::writer(sink)).writeString(protocolName);
}

std::string TcpConnector::toString() const {
  char buf[128];
  int n = snprintf(buf, sizeof(buf), "TcpConnector/%s@%p[%s:%d]",
                   name().c_str(), this, bindAddress_.c_str(), port_);
  return std::string(buf, n);
}

template<> std::string StringUtil::toString(TcpConnector* c) {
  return c->toString();
}

}  // namespace xzero
