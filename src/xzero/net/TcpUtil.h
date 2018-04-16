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
#include <functional>
#include <string>

namespace xzero {

class Executor;
class FileView;
class Socket;
class TcpConnection;

class TcpUtil {
public:
  using ConnectionFactory = std::function<TcpConnection*(const std::string&)>;

  static std::error_code connect(Socket& socket, const InetAddress& remote);

  static void setLingering(int fd, Duration d);
};

}  // namespace xzero
