// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/thread/Future.h>
#include <xzero/net/InetAddress.h>
#include <xzero/Duration.h>
#include <system_error>

namespace xzero {

class Executor;

class InetUtil {
 public:
  static int getLocalPort(int socket, int addressFamily);

  static Result<int> openTcpSocket(int addressFamily);

  static Future<int> connect(const InetAddress& remote,
                             Duration timeout,
                             Executor* executor);

  static std::error_code connect(int socket, const InetAddress& remote);
};

}  // namespace xzero
