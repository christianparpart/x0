// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/http1/Generator.h>
#include <xzero/http/http1/Parser.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/net/EndPoint.h>
#include <xzero/executor/Scheduler.h>
#include <xzero/Buffer.h>
#include <string>

namespace xzero {
namespace http {

/**
 * HTTP client response object.
 */
class ClientResponse { // {{{
 public:
  ClientResponse();
  ~ClientResponse();

  HttpVersion version() const noexcept { return version_; }
  HttpStatus status() const noexcept { return status_; }
  const HeaderFieldList& headers() const noexcept { return headers_; }
  const Buffer& body() const noexcept { return body_; }

 private:
  HttpVersion version_;
  HttpStatus status_;
  HeaderFieldList headers_;
  Buffer body_;
}; // }}}

/**
 * HTTP client API.
 *
 * Requirements:
 * - I/O timeouts
 * - sync & async calls.
 * - incremental response receive vs full response object
 */
class XZERO_HTTP_API Client {
 public:
  Client(HttpVersion version,
         const std::string& method,
         const std::string& entity,
         const HeaderFieldList& headers,
         FileView&& body);

  Client(HttpVersion version,
         const std::string& method,
         const std::string& entity,
         const HeaderFieldList& headers,
         Buffer&& body);

  Client(HttpVersion version,
         const std::string& method,
         const std::string& entity,
         const HeaderFieldList& headers,
         const BufferRef& body);

  Client(HttpVersion version,
         const std::string& method,
         const std::string& entity,
         const HeaderFieldList& headers);

  void send(const IPAddress& host, int port, HttpListener* response);
  void send(const std::string& unixsockpath, HttpListener* response);

  static void send(HttpVersion version, const std::string& method,
                   const std::string& entity, const HeaderFieldList& headers,
                   HttpListener* responseListener);

 private:
  Scheduler* scheduler_;
  EndPoint* endpoint_;
  EndPointWriter writer_;
  Generator generator_;
  Parser parser_;
};

}  // namespace http
}  // namespace xzero
