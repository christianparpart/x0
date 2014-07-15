// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <x0/IPAddress.h>
#include <x0/http/HttpMessageParser.h>
#include <unordered_map>
#include <functional>
#include <initializer_list>
#include <string>
#include <ev++.h>

namespace x0 {

class Socket;

enum class HttpClientError {
  Success = 0,
  ConnectError,
  WriteError,
  ReadError,
  ProtocolError,
};

std::string tos(HttpClientError ec);

class X0_API HttpClient : public HttpMessageParser {
 public:
  typedef std::unordered_map<std::string, std::string> HeaderMap;

  typedef std::function<void(HttpClientError, int /*status*/,
                             const HeaderMap& /*headers*/,
                             const BufferRef& /*content*/)> ResponseHandler;

  explicit HttpClient(ev::loop_ref loop, const IPAddress& ipaddr, int port);
  ~HttpClient();

  void setResultCallback(const ResponseHandler& callback) {
    responseHandler_ = callback;
  }

  void setRequest(const std::string& method, const std::string& path,
                  const HeaderMap& headers = HeaderMap(),
                  const Buffer& content = Buffer());

  void setRequest(
      const std::string& method, const std::string& path,
      const std::initializer_list<
          std::pair<const std::string&, const std::string&>>& headers,
      const Buffer& content = Buffer());

  void start();

  void stop();

  static void request(
      const IPAddress& host, int port, const std::string& method,
      const std::string& path,
      const std::initializer_list<std::pair<std::string, std::string>>& headers,
      const Buffer& content, ResponseHandler callback);

  static void request(
      const IPAddress& host, int port, const std::string& method,
      const std::string& path,
      const std::unordered_map<std::string, std::string>& headers,
      const Buffer& content, ResponseHandler callback);

  void log(LogMessage&& msg) override;

 protected:
  void reportError(HttpClientError ec);
  void onConnectDone(Socket* socket, int revents);
  void io(Socket* socket, int revents);
  void readSome();
  void writeSome();

  bool onMessageBegin(int versionMajor, int versionMinor, int code,
                      const BufferRef& text) override;
  bool onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  bool onMessageHeaderEnd() override;
  bool onMessageContent(const BufferRef& chunk) override;
  bool onMessageEnd() override;

 protected:
  ev::loop_ref loop_;
  IPAddress ipaddr_;
  int port_;
  Socket* socket_;

  // raw request
  Buffer writeBuffer_;
  size_t writeOffset_;

  // raw response
  Buffer readBuffer_;
  size_t readOffset_;

  // prepared response
  bool processingDone_;
  int statusCode_;
  BufferRef statusText_;
  HeaderMap headers_;
  Buffer content_;

  ResponseHandler responseHandler_;
};

// {{{ inlines
inline std::string tos(HttpClientError ec) {
  switch (ec) {
    case HttpClientError::Success:
      return "Success";
    case HttpClientError::ConnectError:
      return "Connecting to server failed";
    case HttpClientError::WriteError:
      return "Writing to server failed";
    case HttpClientError::ReadError:
      return "Reading from server failed";
    case HttpClientError::ProtocolError:
      return "Protocol interpretation failed";
    default:
      return "Unknown";
  }
}

inline void HttpClient::setRequest(
    const std::string& method, const std::string& path,
    const std::initializer_list<
        std::pair<const std::string&, const std::string&>>& headers,
    const Buffer& content) {
  std::unordered_map<std::string, std::string> headerMap;
  for (auto item : headers) headerMap[item.first] = item.second;

  setRequest(method, path, headerMap, content);
}

inline void HttpClient::request(
    const IPAddress& host, int port, const std::string& method,
    const std::string& path,
    const std::initializer_list<std::pair<std::string, std::string>>& headers,
    const Buffer& content, ResponseHandler callback) {
  std::unordered_map<std::string, std::string> headerMap;
  for (auto item : headers) headerMap[item.first] = item.second;

  request(host, port, method, path, headerMap, content, callback);
}
// }}}

}  // namespace x0
