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
  left_ = Socket::make_socket(FileDescriptor(sv[0]), Socket::AddressFamily(PF_UNIX));
  right_ = Socket::make_socket(FileDescriptor(sv[1]), Socket::AddressFamily(PF_UNIX));

  if (flags) {
    if (fcntl(sv[0], F_SETFL, flags) < 0)
      RAISE_ERRNO(errno);
    if (fcntl(sv[1], F_SETFL, flags) < 0)
      RAISE_ERRNO(errno);
  }

#elif defined(XZERO_OS_WINDOWS)

#error "TODO"

#endif
}

SocketPair::~SocketPair() {
}

} // namespace xzero
