// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/io/FileDescriptor.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/RefPtr.h>
#include <xzero/Duration.h>
#include <system_error>
#include <functional>
#include <string>

namespace xzero {

class TcpEndPoint;
class Executor;
class SslContext;
class SslConnector;
class SslEndPoint;

class SslErrorCategory : public std::error_category {
 public:
  const char* name() const noexcept override;
  std::string message(int ev) const override;

  static std::error_category& get();
};

class SslUtil {
 public:
  using ConnectionFactory = std::function<void(const std::string&, SslEndPoint*)>;

  static void initialize();

  static std::error_code error(int ev);

  static RefPtr<SslEndPoint> accept(
      FileDescriptor&& fd,
      int addressFamily,
      SslConnector* connector,
      ConnectionFactory connectionFactory,
      Executor* executor);

  static RefPtr<SslEndPoint> accept(
      FileDescriptor&& fd,
      int addressFamily,
      Duration readTimeout,
      Duration writeTimeout,
      SslContext* defaultContext,
      std::function<void(TcpEndPoint*)> onEndPointClosed,
      ConnectionFactory connectionFactory,
      Executor* executor);

  static Future<RefPtr<SslEndPoint>> connect(
      const std::string& sni,
      const std::vector<std::string>& appProtocols,
      Duration readTimeout,
      Duration writeConnect,
      Executor* executor);

  static Buffer makeProtocolList(const std::list<std::string>& protos);
};

} // namespace xzero
