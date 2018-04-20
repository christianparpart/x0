// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RuntimeError.h>
#include <xzero/defines.h>
#include <xzero/net/Socket.h>
#include <xzero/net/SocketPair.h>
#include <xzero/sysconfig.h>

#if defined(XZERO_OS_UNIX)
#include <fcntl.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#include <winsock2.h>
#endif

namespace xzero {

SocketPair::SocketPair()
    : SocketPair{Blocking} {
}

SocketPair::SocketPair(BlockingMode blockingMode)
    : left_{Socket::InvalidSocket},
      right_{Socket::InvalidSocket} {
#if defined(XZERO_OS_UNIX)
  int typeMask = 0;
  int flags = 0;
#if defined(SOCK_CLOEXEC)
  typeMask |= SOCK_CLOEXEC;
#else
  flags |= O_CLOEXEC;
#endif

  if (blockingMode == NonBlocking) {
#if defined(SOCK_NONBLOCK)
    typeMask |= SOCK_NONBLOCK;
#else
    flags |= O_NONBLOCK;
#endif
  }

  int sv[2];
  int rv = socketpair(PF_UNIX, SOCK_STREAM | typeMask, 0, sv);
  left_ = Socket::make_socket(Socket::AddressFamily(PF_UNIX), FileDescriptor(sv[0]));
  right_ = Socket::make_socket(Socket::AddressFamily(PF_UNIX), FileDescriptor(sv[1]));

  if (flags) {
    if (fcntl(sv[0], F_SETFL, flags) < 0)
      RAISE_ERRNO(errno);
    if (fcntl(sv[1], F_SETFL, flags) < 0)
      RAISE_ERRNO(errno);
  }

#elif defined(XZERO_OS_WINDOWS)
  Socket srv = Socket::make_tcp_ip(true);

  sockaddr_in sin;
  ZeroMemory(&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
  sin.sin_port = 0;

  int reuse = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
  bind(srv.native(), (const sockaddr*) &sin, sizeof(sin));

  socklen_t addrlen = sizeof(sin.sin_addr);
  getsockname(srv.native(), (sockaddr*) &sin.sin_addr, &addrlen);
  listen(srv.native(), 1);

  left_ = Socket::make_tcp_ip(true);
  connect(left_.native(), (sockaddr*)&sin.sin_addr, sizeof(sin.sin_addr));

  right_ = Socket::make_socket(Socket::AddressFamily::V4, accept(srv.native(), nullptr, nullptr));

  // TODO: error handling
#endif
}

SocketPair::~SocketPair() {
}

} // namespace xzero
