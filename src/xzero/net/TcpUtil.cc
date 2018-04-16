// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/TcpUtil.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Socket.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/executor/Executor.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#if defined(XZERO_OS_WINDOWS)
#include <Windows.h>
#include <WinSock2.h>
#endif

#if defined(XZERO_OS_UNIX)
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#endif

#if defined(HAVE_SYS_SENDFILE_H)
#include <sys/sendfile.h>
#endif

namespace xzero {

std::error_code TcpUtil::connect(Socket& sd, const InetAddress& address) {
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

      rv = ::connect(sd, (const struct sockaddr*) &saddr, sizeof(saddr));
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

      rv = ::connect(sd, (const struct sockaddr*) &saddr, sizeof(saddr));
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

bool TcpUtil::isTcpNoDelay(int fd) {
  int result = 0;
  socklen_t sz = sizeof(result);
  if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &result, &sz) < 0)
    RAISE_ERRNO(errno);

  return result;
}

void TcpUtil::setTcpNoDelay(int fd, bool enable) {
  int flag = enable ? 1 : 0;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
}

bool TcpUtil::isCorking(int fd) {
#if defined(TCP_CORK)
  int flag = 0;
  socklen_t sz = sizeof(flag);
  if (getsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, &sz) < 0)
    RAISE_ERRNO(errno);

  return flag;
#else
  return false;
#endif
}

void TcpUtil::setCorking(int fd, bool enable) {
#if defined(TCP_CORK)
  int flag = enable ? 1 : 0;
  if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
#endif
}

void TcpUtil::setLingering(int fd, Duration d) {
#if defined(TCP_LINGER2)
  int waitTime = d.seconds();
  if (!waitTime)
    return;

  if (setsockopt(fd, SOL_TCP, TCP_LINGER2, &waitTime, sizeof(waitTime)) < 0)
    RAISE_ERRNO(errno);
#endif
}

} // namespace xzero
