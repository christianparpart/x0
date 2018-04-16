// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/UdpConnector.h>
#include <xzero/net/UdpEndPoint.h>
#include <xzero/net/InetAddress.h>
#include <xzero/io/FileUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

#include <fcntl.h>
#include <sys/types.h>

#if defined(XZERO_OS_UNIX)
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#include <io.h>
#include <WinSock2.h>
#endif

namespace xzero {

#if defined(XZERO_OS_LINUX) && !defined(SO_REUSEPORT)
#define SO_REUSEPORT 15
#endif

UdpConnector::UdpConnector(const std::string& name,
                           Handler handler,
                           Executor* executor,
                           const InetAddress& address,
                           bool reuseAddr,
                           bool reusePort)
    : name_{name},
      handler_{handler},
      executor_{executor},
      socket_{Socket::make_udp_ip(true)} {
  open(address, reuseAddr, reusePort);

  BUG_ON(executor == nullptr);
}

UdpConnector::~UdpConnector() {
  if (isStarted()) {
    stop();
  }
}

void UdpConnector::start() {
  notifyOnEvent();
}

bool UdpConnector::isStarted() const {
  return io_.get() != nullptr;
}

void UdpConnector::stop() {
  if (!isStarted())
    throw std::logic_error{"Trying to stop an UdpConnector that has not been started."};

  if (io_) {
    io_->cancel();
    io_ = nullptr;
  }
}

void UdpConnector::open(const InetAddress& address, bool reuseAddr, bool reusePort) {
#if defined(SO_REUSEPORT)
  if (reusePort) {
    int rc = 1;
    if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, (const char*) &rc, sizeof(rc)) < 0) {
      RAISE_ERRNO(errno);
    }
  }
#endif

  if (reuseAddr) {
    int rc = 1;
    if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (const char*) &rc, sizeof(rc)) < 0) {
      RAISE_ERRNO(errno);
    }
  }

  // bind
  char sa[sizeof(sockaddr_in6)];
  socklen_t salen = address.ip().size();

  switch (address.family()) {
    case IPAddress::Family::V4:
      salen = sizeof(sockaddr_in);
      ((sockaddr_in*) sa)->sin_port = htons(address.port());
      ((sockaddr_in*) sa)->sin_family = AF_INET;
      memcpy(&((sockaddr_in*)sa)->sin_addr, address.ip().data(), address.ip().size());
      break;
    case IPAddress::Family::V6:
      salen = sizeof(sockaddr_in6);
      ((sockaddr_in6*) sa)->sin6_port = htons(address.port());
      ((sockaddr_in6*) sa)->sin6_family = AF_INET6;
      memcpy(&((sockaddr_in6*)sa)->sin6_addr, address.ip().data(), address.ip().size());
      break;
    default:
      logFatal("Invalid IPAddress::Family");
  }

  int rv = ::bind(socket_, (sockaddr*)sa, salen);
  if (rv < 0)
    RAISE_ERRNO(errno);
}

void UdpConnector::notifyOnEvent() {
  logTrace("UdpConnector: notifyOnEvent()");
  io_ = executor_->executeOnReadable(
      socket_,
      std::bind(&UdpConnector::onMessage, this));
}

void UdpConnector::onMessage() {
  logTrace("UdpConnector: onMessage");

  socklen_t remoteAddrLen;
  sockaddr* remoteAddr;
  sockaddr_in sin4;
  sockaddr_in6 sin6;

  switch (socket_.addressFamily()) {
    case IPAddress::Family::V4:
      remoteAddr = (sockaddr*) &sin4;
      remoteAddrLen = sizeof(sin4);
      break;
    case IPAddress::Family::V6:
      remoteAddr = (sockaddr*) &sin6;
      remoteAddrLen = sizeof(sin6);
      break;
    default:
      logFatal("Invalid IP address family.");
  }

  Buffer message;
  message.reserve(65535);

  notifyOnEvent();

  int n;
  do {
    n = recvfrom(
        socket_,
        message.data(),
        message.capacity(),
        0,
        remoteAddr,
        &remoteAddrLen);
  } while (n < 0 && errno == EINTR);

  if (n < 0)
    RAISE_ERRNO(errno);

  if (handler_) {
    message.resize(n);
    auto client = std::make_shared<UdpEndPoint>(
        this, std::move(message), remoteAddr, remoteAddrLen);
    executor_->execute(std::bind(handler_, client));
  } else {
    logTrace("UdpConnector: Ignoring incoming message of {} bytes. No handler set.", n);
  }
}

} // namespace xzero
