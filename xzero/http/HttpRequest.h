// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/Buffer.h>
#include <xzero/HugeBuffer.h>
#include <xzero/io/File.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/http/HttpMethod.h>
#include <xzero/net/InetAddress.h>
#include <xzero/Option.h>
#include <memory>

namespace xzero {

class InputStream;

namespace http {

/**
 * Represents an HTTP request message.
 */
class XZERO_HTTP_API HttpRequest : public HttpRequestInfo {
 public:
  HttpRequest();
  HttpRequest(const std::string& method, const std::string& path,
              HttpVersion version, bool secure, const HeaderFieldList& headers,
              Buffer&& content);

  void setRemoteAddress(const Option<InetAddress>& addr);
  const Option<InetAddress>& remoteAddress() const;

  void setLocalAddress(const Option<InetAddress>& addr);
  const Option<InetAddress>& localAddress() const;

  size_t bytesReceived() const noexcept { return bytesReceived_; }
  void setBytesReceived(size_t n) { bytesReceived_ = n; }

  const std::string& host() const noexcept { return host_; }
  void setHost(const std::string& value);

  bool isSecure() const noexcept { return secure_; }
  void setSecure(bool secured) { secure_ = secured; }

  bool expect100Continue() const noexcept { return expect100Continue_; }
  void setExpect100Continue(bool value) noexcept { expect100Continue_ = value; }

  const std::string& username() const noexcept { return username_; }
  void setUserName(const std::string& value) { username_ = value; }

  void recycle();

  // request body handling
  void discardContent(std::function<void()> onReady);
  void consumeContent(std::function<void()> onReady);
  void fillContent(const BufferRef& chunk);
  void ready();

  std::unique_ptr<InputStream> getContentStream();
  BufferRef getContentBuffer();

 private:
  Option<InetAddress> remoteAddress_;
  Option<InetAddress> localAddress_;
  size_t bytesReceived_;

  std::string host_;
  bool secure_;

  bool expect100Continue_;
  HugeBuffer content_;
  std::function<void()> onContentReady_;
  std::function<void(const BufferRef&)> onContentAvailable_;

  std::string username_; // the client's username, if authenticated
};

}  // namespace http
}  // namespace xzero
