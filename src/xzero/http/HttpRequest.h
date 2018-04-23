// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/HugeBuffer.h>
#include <xzero/io/File.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/http/HttpMethod.h>
#include <xzero/net/InetAddress.h>
#include <optional>
#include <memory>

namespace xzero {

class InputStream;

namespace http {

/**
 * Represents an HTTP request message.
 */
class HttpRequest : public HttpRequestInfo {
 public:
  HttpRequest();
  HttpRequest(HttpVersion version,
              HttpMethod method,
              const std::string& uri,
              const HeaderFieldList& headers,
              bool secure,
              HugeBuffer&& content);
  HttpRequest(HttpVersion version,
              const std::string& method,
              const std::string& uri,
              const HeaderFieldList& headers,
              bool secure,
              HugeBuffer&& content);
  HttpRequest(const HttpRequestInfo& info, HugeBuffer&& content);

  void setRemoteAddress(const std::optional<InetAddress>& addr);
  const std::optional<InetAddress>& remoteAddress() const;

  void setLocalAddress(const std::optional<InetAddress>& addr);
  const std::optional<InetAddress>& localAddress() const;

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

  // {{{ Asynchronous request body handler API
  /**
   * Discards the request body and invokes @p onReady once discarded.
   */
  void discardContent(std::function<void()> onReady);

  /**
   * Consumes the request body and invokes @p onReady once fully available.
   */
  void consumeContent(std::function<void()> onReady);

  /**
   * Adds a chunk to the request body, progressively populting it.
   */
  void fillContent(const BufferRef& chunk);

  /**
   * Invoke if the request body has been fully populated.
   */
  void ready();

  HugeBuffer& getContent();
  const HugeBuffer& getContent() const;
  // }}}

 private:
  std::optional<InetAddress> remoteAddress_;
  std::optional<InetAddress> localAddress_;
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
