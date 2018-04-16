// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
//
#include <xzero/RuntimeError.h>
#include <xzero/defines.h>
#include <xzero/net/Socket.h>
#include <xzero/sysconfig.h>

#if defined(XZERO_OS_UNIX)
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#include <winsock2.h>
#endif

namespace xzero {

Socket::Socket(NonBlockingTcpCtor, AddressFamily af) : Socket{af, TCP, NonBlocking} {}

#if defined(XZERO_OS_UNIX)
Socket::Socket(AddressFamily af, FileDescriptor&& fd)
    : handle_(std::move(fd)), addressFamily_(af) {
}
#endif

Socket::Socket(AddressFamily af, Type type, BlockingMode bm) {
#if defined(XZERO_OS_UNIX)
  const int proto = type == TCP ? IPPROTO_TCP : IPPROTO_UDP;
  const int ty = type == TCP ? SOCK_STREAM : SOCK_DGRAM;

  int flags = 0;
#if defined(SOCK_CLOEXEC)
  flags |= SOCK_CLOEXEC;
#endif
#if defined(SOCK_NONBLOCK)
  if (bm & NonBlocking) {
    flags |= SOCK_NONBLOCK;
  }
#endif

  FileDescriptor fd = socket(static_cast<int>(af), ty | flags, proto);
  if (fd < 0)
    RAISE_ERRNO(errno);

#if !defined(SOCK_NONBLOCK)
  flags = enable ? fcntl(fd, F_GETFL) & ~O_NONBLOCK
                 : fcntl(fd, F_GETFL) | O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
#endif

  handle_ = std::move(fd);
  addressFamily_ = af;
#elif defined(XZERO_OS_WINDOWS) // TODO
#endif
}

Socket::~Socket() {
}

Socket::Socket(InvalidSocketState) :
#if defined(XZERO_OS_UNIX)
    handle_{ -1 },
#elif defined(XZERO_OS_WINDOWS)
    handle_{ INVALID_SOCKET },
#endif
    addressFamily_{} {
}

Socket::Socket(Socket&& other)
    : handle_{std::move(other.handle_)},
      addressFamily_{std::move(other.addressFamily_)} {
#if defined(XZERO_OS_WINDOWS)
  other.handle_ = INVALID_SOCKET;
#endif
}

Socket& Socket::operator=(Socket&& other) {
#if defined(XZERO_OS_UNIX)
  handle_ = std::move(other.handle_);
  addressFamily_ = std::move(other.addressFamily_);
#elif defined(XZERO_OS_WINDOWS) // TODO
#endif
  return *this;
}

bool Socket::valid() const noexcept {
#if defined(XZERO_OS_UNIX)
  return handle_.isOpen();
#else
  return handle_ != INVALID_SOCKET;
#endif
}

void Socket::close() {
#if defined(XZERO_OS_UNIX)
  handle_.close();
#else
  closesocket(handle_);
  handle_ = INVALID_SOCKET;
#endif
}

Socket Socket::make_tcp_ip(bool nonBlocking, AddressFamily af) {
  return Socket(af, TCP, nonBlocking ? NonBlocking : Blocking);
}

Socket Socket::make_udp_ip(bool nonBlocking, AddressFamily af) {
  return Socket(af, UDP, nonBlocking ? NonBlocking : Blocking);
}

#if defined(XZERO_OS_UNIX)
Socket Socket::make_socket(FileDescriptor&& fd, AddressFamily af) {
  return Socket(af, std::move(fd));
}
#endif

int Socket::getLocalPort() const {
  switch (addressFamily_) {
    case IPAddress::Family::V6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);
      if (getsockname(handle_, (sockaddr*) &saddr, &slen) < 0)
        RAISE_ERRNO(errno);

      return ntohs(saddr.sin6_port);
    }
    case IPAddress::Family::V4: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);
      if (getsockname(handle_, (sockaddr*) &saddr, &slen) < 0)
        RAISE_ERRNO(errno);

      return ntohs(saddr.sin_port);
    }
    default: {
      throw std::logic_error{"getLocalPort() invoked on invalid address family"};
    }
  }
}

Result<InetAddress> Socket::getLocalAddress() const {
  if (handle_ < 0)
    return static_cast<std::errc>(EINVAL);

  switch (addressFamily_) {
    case IPAddress::Family::V6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);

      if (getsockname(handle_, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin6_port)));
    }
    case IPAddress::Family::V4: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);

      if (getsockname(handle_, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin_port)));
    }
    default:
      return static_cast<std::errc>(EINVAL);
  }
}
Result<InetAddress> Socket::getRemoteAddress() const {
  if (handle_ < 0)
    return static_cast<std::errc>(EINVAL);

  switch (addressFamily_) {
    case IPAddress::Family::V6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);
      if (getpeername(handle_, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin6_port)));
    }
    case IPAddress::Family::V4: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);
      if (getpeername(handle_, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin_port)));
    }
    default:
      return static_cast<std::errc>(EINVAL);
  }
}

int Socket::write(const void* buf, size_t count) {
  // TODO: error handling (throw?)
#if defined(XZERO_OS_UNIX)
  return ::write(handle_, buf, count);
#elif defined(XZERO_OS_WINDOWS)
  DWORD nwritten = 0;
  DWORD flags = 0;
  if (::WSASend(handle_, buf, count, &nwritten, flags, nullptr, nullptr) == SOCKET_ERROR)
    return -1;
  return nwritten;
#endif
}

void Socket::setBlocking(bool enable) {
#if defined(XZERO_OS_UNIX)
  unsigned flags = enable ? fcntl(handle_, F_GETFL) & ~O_NONBLOCK
                          : fcntl(handle_, F_GETFL) | O_NONBLOCK;

  if (fcntl(handle_, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
#elif defined(XZERO_OS_WINDOWS)
#error "TODO"
#else
#error "Unknown platform"
#endif
}

} // namespace xzero
