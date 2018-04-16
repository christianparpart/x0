// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/UdpClient.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <stdexcept>

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

UdpClient::UdpClient(const InetAddress& address)
    : socket_{Socket::make_udp_ip(false)},
      sockAddr_{nullptr},
      sockAddrLen_{0}
{
  switch (address.family()) {
    case IPAddress::Family::V4:
      sockAddrLen_ = sizeof(sockaddr_in);

      sockAddr_ = malloc(sockAddrLen_);
      if (!sockAddr_)
        throw std::bad_alloc{};

      memset(sockAddr_, 0, sockAddrLen_);

      ((sockaddr_in*) sockAddr_)->sin_port = htons(address.port());
      ((sockaddr_in*) sockAddr_)->sin_family = AF_INET;
      memcpy(&((sockaddr_in*)sockAddr_)->sin_addr,
             address.ip().data(), address.ip().size());
      break;
    case IPAddress::Family::V6:
      sockAddrLen_ = sizeof(sockaddr_in6);

      sockAddr_ = malloc(sockAddrLen_);
      if (!sockAddr_)
        throw std::bad_alloc{};

      memset(sockAddr_, 0, sockAddrLen_);

      ((sockaddr_in6*) sockAddr_)->sin6_port = htons(address.port());
      ((sockaddr_in6*) sockAddr_)->sin6_family = AF_INET6;
      memcpy(&((sockaddr_in6*)sockAddr_)->sin6_addr,
             address.ip().data(), address.ip().size());
      break;
    default:
      logFatal("Invalid IPAddress.family()");
  }
}

UdpClient::~UdpClient() {
  free(sockAddr_);
}

size_t UdpClient::send(const BufferRef& message) {
  const int flags = 0;
  ssize_t n;

  do {
    n = sendto(
        socket_,
        message.data(),
        message.size(),
        flags,
        (sockaddr*) sockAddr_,
        sockAddrLen_);
  } while (n < 0 && errno == EINTR);

  if (n < 0)
    RAISE_ERRNO(errno);

  return n;
}

size_t UdpClient::receive(Buffer* message) {
  int n;

  message->reserve(65536);
  socklen_t socklen = (socklen_t) sockAddrLen_;

  do {
    n = recvfrom(
        socket_,
        message->data(),
        message->capacity(),
        0,
        (sockaddr*) sockAddr_,
        &socklen);
  } while (n < 0 && errno == EINTR);

  if (n < 0)
    RAISE_ERRNO(errno);

  return static_cast<size_t>(n);
}

} // namespace xzero
