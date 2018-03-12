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
#include <string>

namespace xzero {

class FileView;
class Executor;
class TcpConnection;

class TcpUtil {
 public:
  using ConnectionFactory = std::function<TcpConnection*(const std::string&)>;

  static Result<int> openTcpSocket(int addressFamily);

  static Future<int> connect(const InetAddress& remote,
                             Duration timeout,
                             Executor* executor);

  static XZERO_NODISCARD std::error_code connect(int socket, const InetAddress& remote);

  static XZERO_NODISCARD Result<InetAddress> getLocalAddress(int fd, int addressFamily);
  static XZERO_NODISCARD Result<InetAddress> getRemoteAddress(int fd, int addressFamily);
  static int getLocalPort(int socket, int addressFamily);

  static bool isTcpNoDelay(int fd);
  static void setTcpNoDelay(int fd, bool enable);

  static bool isCorking(int fd);
  static void setCorking(int fd, bool enable);

  static size_t sendfile(int target, const FileView& source);
};

}  // namespace xzero
