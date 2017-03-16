// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/InetUtil.h>
#include <xzero/RuntimeError.h>

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

namespace xzero {

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

} // namespace xzero
