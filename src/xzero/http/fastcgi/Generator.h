// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HttpInfo.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/fastcgi/bits.h>
#include <xzero/io/FileView.h>
#include <xzero/Buffer.h>

namespace xzero {

class EndPointWriter;

namespace http {
namespace fastcgi {

/**
 * FastCGI Request/Response stream generator.
 *
 * Generates the binary stream for an HTTP request or response.
 */
class XZERO_HTTP_API Generator {
 public:
  /**
   * Generates an HTTP request or HTTP response for given FastCGI request-ID.
   *
   * @param requestId FastCGI's requestId that is associated for the generated message.
   * @param dateGenerator used for creating the Date message header.
   * @param writer endpoint writer to write the binary stream to.
   */
  Generator(
      int requestId,
      EndPointWriter* writer);

  void generateRequest(const HttpRequestInfo& info);
  void generateRequest(const HttpRequestInfo& info, Buffer&& chunk);
  void generateRequest(const HttpRequestInfo& info, const BufferRef& chunk);
  void generateResponse(const HttpResponseInfo& info);
  void generateResponse(const HttpResponseInfo& info, const BufferRef& chunk);
  void generateResponse(const HttpResponseInfo& info, Buffer&& chunk);
  void generateResponse(const HttpResponseInfo& info, FileView&& chunk);
  void generateBody(Buffer&& chunk);
  void generateBody(const BufferRef& chunk);
  void generateBody(FileView&& chunk);
  void generateEnd();

  void flushBuffer();

  size_t bytesTransmitted() const noexcept { return bytesTransmitted_; }

 private:
  void write(Type type, int requestId, const char* buf, size_t len);

 private:
  enum Mode { Nothing, GenerateRequest, GenerateResponse } mode_;
  int requestId_;
  size_t bytesTransmitted_;
  Buffer buffer_;
  EndPointWriter* writer_;
};

} // namespace fastcgi
} // namespace http
} // namespace xzero
