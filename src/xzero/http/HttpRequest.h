// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/sysconfig.h>
#include <xzero/Buffer.h>
#include <xzero/io/File.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/http/HttpMethod.h>
#include <xzero/http/HttpInput.h>
#include <xzero/net/InetAddress.h>
#include <xzero/Option.h>
#include <memory>

namespace xzero {
namespace http {

/**
 * Represents an HTTP request message.
 */
class XZERO_HTTP_API HttpRequest : public HttpRequestInfo {
 public:
  HttpRequest();
  explicit HttpRequest(std::unique_ptr<HttpInput>&& input);
  HttpRequest(const std::string& method, const std::string& path,
              HttpVersion version, bool secure, const HeaderFieldList& headers,
              std::unique_ptr<HttpInput>&& input);

  void setRemoteAddress(const Option<InetAddress>& addr);
  const Option<InetAddress>& remoteAddress() const;

  void setLocalAddress(const Option<InetAddress>& addr);
  const Option<InetAddress>& localAddress() const;

  size_t bytesReceived() const noexcept { return bytesReceived_; }
  void setBytesReceived(size_t n) { bytesReceived_ = n; }

  const std::string& host() const XZERO_NOEXCEPT { return host_; }
  void setHost(const std::string& value);

  bool isSecure() const XZERO_NOEXCEPT { return secure_; }
  void setSecure(bool secured) { secure_ = secured; }

  HttpInput* input() const { return input_.get(); }
  void setInput(std::unique_ptr<HttpInput>&& input) { input_ = std::move(input); }

  bool expect100Continue() const XZERO_NOEXCEPT { return expect100Continue_; }
  void setExpect100Continue(bool value) XZERO_NOEXCEPT { expect100Continue_ = value; }

  const std::string& username() const noexcept { return username_; }
  void setUserName(const std::string& value) { username_ = value; }

  void recycle();

 private:
  Option<InetAddress> remoteAddress_;
  Option<InetAddress> localAddress_;
  size_t bytesReceived_;

  bool secure_;
  bool expect100Continue_;
  std::string host_;

  std::unique_ptr<HttpInput> input_;

  std::string username_; // the client's username, if authenticated
};

}  // namespace http
}  // namespace xzero
