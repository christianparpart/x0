// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/UdpConnector.h>
#include <xzero/net/UdpEndPoint.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

namespace xzero {

#if !defined(SO_REUSEPORT)
#define SO_REUSEPORT 15
#endif

UdpConnector::UdpConnector(
    const std::string& name,
    DatagramHandler handler,
    Executor* executor,
    const IPAddress& ipaddr, int port,
    bool reuseAddr, bool reusePort)
    : DatagramConnector(name, handler, executor),
      socket_(-1),
      addressFamily_(0) {
  open(ipaddr, port, reuseAddr, reusePort);

  BUG_ON(executor == nullptr);
}

UdpConnector::~UdpConnector() {
  if (isStarted()) {
    stop();
  }

  if (socket_ >= 0) {
    FileUtil::close(socket_);
    socket_ = -1;
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
    RAISE(IllegalStateError);

  if (io_) {
    io_->cancel();
    io_ = nullptr;
  }
}

void UdpConnector::open(
    const IPAddress& ipaddr, int port,
    bool reuseAddr, bool reusePort) {

  socket_ = ::socket(ipaddr.family(), SOCK_DGRAM, 0);
  addressFamily_ = ipaddr.family();

  if (socket_ < 0)
    RAISE_ERRNO(errno);

  if (reusePort) {
    int rc = 1;
    if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &rc, sizeof(rc)) < 0) {
      RAISE_ERRNO(errno);
    }
  }

  if (reuseAddr) {
    int rc = 1;
    if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof(rc)) < 0) {
      RAISE_ERRNO(errno);
    }
  }

  // bind(ipaddr, port);
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
}

void UdpConnector::notifyOnEvent() {
  logTrace("UdpConnector", "notifyOnEvent()");
  io_ = executor_->executeOnReadable(
      socket_,
      std::bind(&UdpConnector::onMessage, this));
}

void UdpConnector::onMessage() {
  logTrace("UdpConnector", "onMessage");

  socklen_t remoteAddrLen;
  sockaddr* remoteAddr;
  sockaddr_in sin4;
  sockaddr_in6 sin6;

  switch (addressFamily_) {
    case IPAddress::V4:
      remoteAddr = (sockaddr*) &sin4;
      remoteAddrLen = sizeof(sin4);
      break;
    case IPAddress::V6:
      remoteAddr = (sockaddr*) &sin6;
      remoteAddrLen = sizeof(sin6);
      break;
    default:
      RAISE(IllegalStateError);
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
    RefPtr<DatagramEndPoint> client(new UdpEndPoint(
        this, std::move(message), remoteAddr, remoteAddrLen));
    executor_->execute(std::bind(handler_, client));
  } else {
    logTrace("UdpConnector",
             "ignoring incoming message of %i bytes. No handler set.", n);
  }
}

} // namespace xzero
