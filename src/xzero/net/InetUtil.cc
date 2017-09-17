// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/InetUtil.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/executor/Executor.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

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

#if defined(HAVE_SYS_SENDFILE_H)
#include <sys/sendfile.h>
#endif

namespace xzero {

#define TRACE(msg...) logTrace("InetUtil", msg)

Result<InetAddress> InetUtil::getRemoteAddress(int fd, int addressFamily) {
  if (fd < 0)
    return static_cast<std::errc>(EINVAL);

  switch (addressFamily) {
    case AF_INET6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);
      if (getpeername(fd, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin6_port)));
    }
    case AF_INET: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);
      if (getpeername(fd, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin_port)));
    }
    default:
      return static_cast<std::errc>(EINVAL);
  }
}

Result<InetAddress> InetUtil::getLocalAddress(int fd, int addressFamily) {
  if (fd < 0)
    return static_cast<std::errc>(EINVAL);

  switch (addressFamily) {
    case AF_INET6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);

      if (getsockname(fd, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin6_port)));
    }
    case AF_INET: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);

      if (getsockname(fd, (sockaddr*)&saddr, &slen) < 0)
        return static_cast<std::errc>(errno);

      return Success(InetAddress(IPAddress(&saddr), ntohs(saddr.sin_port)));
    }
    default:
      return static_cast<std::errc>(EINVAL);
  }
}

int InetUtil::getLocalPort(int socket, int addressFamily) {
  switch (addressFamily) {
    case AF_INET6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);
      if (getsockname(socket, (sockaddr*)&saddr, &slen) < 0)
        RAISE_ERRNO(errno);

      return ntohs(saddr.sin6_port);
    }
    case AF_INET: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);
      if (getsockname(socket, (sockaddr*)&saddr, &slen) < 0)
        RAISE_ERRNO(errno);

      return ntohs(saddr.sin_port);
    }
    default: {
      RAISE(IllegalStateError, "Invalid address family.");
    }
  }
}

Future<int> InetUtil::connect(const InetAddress& remote,
                              Duration timeout,
                              Executor* executor) {
  Promise<int> promise;

  int fd = socket(remote.family(), SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    promise.failure(static_cast<std::errc>(errno));
    return promise.future();
  }

  FileUtil::setBlocking(fd, false);

  std::error_code ec = InetUtil::connect(fd, remote);

  if (!ec) {
    TRACE("InetUtil.connect: connected instantly");
    promise.success(fd);
  } else if (ec == std::errc::operation_in_progress) {
    TRACE("InetUtil.connect: backgrounding");
    executor->executeOnWritable(
        fd,
        [promise, fd]() { promise.success(fd); },
        timeout,
        [promise, fd]() { FileUtil::close(fd);
                          promise.failure(std::errc::timed_out); });
  } else {
    TRACE("InetUtil.connect: failed. $0", ec.message());
    promise.failure(ec);
  }

  return promise.future();
}

std::error_code InetUtil::connect(int fd, const InetAddress& remote) {
  int rv;
  switch (remote.family()) {
    case AF_INET: {
      struct sockaddr_in saddr;
      memset(&saddr, 0, sizeof(saddr));
      saddr.sin_family = remote.family();
      saddr.sin_port = htons(remote.port());
      memcpy(&saddr.sin_addr,
             remote.ip().data(),
             remote.ip().size());

      TRACE("connectAsync: connect(ipv4)");
      rv = ::connect(fd, (const struct sockaddr*) &saddr, sizeof(saddr));
    }
    case AF_INET6: {
      struct sockaddr_in6 saddr;
      memset(&saddr, 0, sizeof(saddr));
      saddr.sin6_family = remote.family();
      saddr.sin6_port = htons(remote.port());
      memcpy(&saddr.sin6_addr,
             remote.ip().data(),
             remote.ip().size());

      TRACE("connectAsync: connect(ipv6)");
      rv = ::connect(fd, (const struct sockaddr*) &saddr, sizeof(saddr));
      break;
    }
    default: {
      return std::make_error_code(std::errc::invalid_argument);
    }
  }

  if (rv < 0)
    return std::make_error_code(static_cast<std::errc>(errno));
  else
    return std::error_code();
}

bool InetUtil::isTcpNoDelay(int fd) {
  int result = 0;
  socklen_t sz = sizeof(result);
  if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &result, &sz) < 0)
    RAISE_ERRNO(errno);

  return result;
}

void InetUtil::setTcpNoDelay(int fd, bool enable) {
  int flag = enable ? 1 : 0;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
}

bool InetUtil::isCorking(int fd) {
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

void InetUtil::setCorking(int fd, bool enable) {
#if defined(TCP_CORK)
  int flag = enable ? 1 : 0;
  if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
#endif
}

size_t InetUtil::sendfile(int target, const FileView& source) {
#if defined(__APPLE__)
  off_t len = source.size();
  int rv = ::sendfile(source.handle(), target, source.offset(), &len, nullptr, 0);
  TRACE("flush(offset:$0, size:$1) -> $2", source.offset(), source.size(), rv);
  if (rv < 0)
    RAISE_ERRNO(errno);

  return len;
#else
  off_t offset = source.offset();
  ssize_t rv = ::sendfile(target, source.handle(), &offset, source.size());
  TRACE("flush(offset:$0, size:$1) -> $2", source.offset(), source.size(), rv);
  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
#endif
}
} // namespace xzero
