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

#if defined(XZERO_OS_WINDOWS)
Socket::Socket(AddressFamily af, SOCKET s)
    : handle_(s), addressFamily_(af) {
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
Socket Socket::make_socket(AddressFamily af, FileDescriptor&& fd) {
  return Socket(af, std::move(fd));
}
#endif

#if defined(XZERO_OS_WINDOWS)
Socket Socket::make_socket(AddressFamily af, SOCKET s) {
  return Socket(af, s);
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
  return ::send(handle_, static_cast<const char*>(buf), count, 0);
}

void Socket::consume() {
  for (;;) {
    char buf[4096];
    int rv = ::recv(handle_, buf, sizeof(buf), 0);
    if (rv < 0) {
      switch (errno) {
        case EBUSY:
        case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        case EWOULDBLOCK:
#endif
          return;
        default:
          RAISE_ERRNO(errno);
      }
    }
  }
}

void Socket::setBlocking(bool enable) {
#if defined(XZERO_OS_UNIX)
  unsigned flags = enable ? fcntl(handle_, F_GETFL) & ~O_NONBLOCK
                          : fcntl(handle_, F_GETFL) | O_NONBLOCK;

  if (fcntl(handle_, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
#elif defined(XZERO_OS_WINDOWS)
  u_long mode = enable ? 1 : 0;
  if (ioctlsocket(handle_, FIONBIO, &mode) == SOCKET_ERROR) {
    RAISE_WSA_ERROR(WSAGetLastError());
  }
#else
#error "Unknown platform"
#endif
}

std::error_code Socket::connect(const InetAddress& address) {
  int rv;
  switch (address.family()) {
    case IPAddress::Family::V4: {
      struct sockaddr_in saddr;
      memset(&saddr, 0, sizeof(saddr));
      saddr.sin_family = static_cast<int>(address.family());
      saddr.sin_port = htons(address.port());
      memcpy(&saddr.sin_addr,
             address.ip().data(),
             address.ip().size());

      rv = ::connect(handle_, (const struct sockaddr*) &saddr, sizeof(saddr));
      break;
    }
    case IPAddress::Family::V6: {
      struct sockaddr_in6 saddr;
      memset(&saddr, 0, sizeof(saddr));
      saddr.sin6_family = static_cast<int>(address.family());
      saddr.sin6_port = htons(address.port());
      memcpy(&saddr.sin6_addr,
             address.ip().data(),
             address.ip().size());

      rv = ::connect(handle_, (const struct sockaddr*) &saddr, sizeof(saddr));
      break;
    }
    default: {
      return std::make_error_code(std::errc::invalid_argument);
    }
  }

  if (rv < 0)
    return make_error_code(static_cast<std::errc>(errno));
  else
    return std::error_code();
}

} // namespace xzero
