// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/io/FileView.h>
#include <xzero/Buffer.h>
#include <memory>

namespace xzero {

class EndPointWriter;

namespace http {

class HttpDateGenerator;
class HttpInfo;
class HttpRequestInfo;
class HttpResponseInfo;

namespace http1 {

/**
 * Generates HTTP/1 messages.
 *
 * Implements the HTTP/1.1 transport layer syntax to generate
 * RFC 7230 conform messages.
 */
class XZERO_HTTP_API Generator {
  enum class State {
    None,
    WritingBody,
    Closed
  };

 public:
  explicit Generator(EndPointWriter* output);

  /** resets any runtime state. */
  void reset();

  /**
   * Generates an HTTP request message.
   *
   * @param info HTTP request message info.
   * @param chunk HTTP message body chunk.
   */
  void generateRequest(const HttpRequestInfo& info, const BufferRef& chunk);
  void generateRequest(const HttpRequestInfo& info, Buffer&& chunk);
  void generateRequest(const HttpRequestInfo& info, FileView&& chunk);
  void generateRequest(const HttpRequestInfo& info);

  /**
   * Generates an HTTP response message.
   *
   * @param info HTTP response message info.
   * @param chunk HTTP message body chunk.
   */
  void generateResponse(const HttpResponseInfo& info, const BufferRef& chunk);
  void generateResponse(const HttpResponseInfo& info, Buffer&& chunk);
  void generateResponse(const HttpResponseInfo& info, FileView&& chunk);

  /**
   * Generates an HTTP message body chunk.
   *
   * @param chunk HTTP message body chunk.
   */
  void generateBody(Buffer&& chunk);

  /**
   * Generates an HTTP message body chunk.
   *
   * @param chunk HTTP message body chunk.
   */
  void generateBody(const BufferRef& chunk);

  /**
   * Generates an HTTP message body chunk.
   *
   * @param chunk HTTP message body chunk, represented as a file.
   */
  void generateBody(FileView&& chunk);

  /**
   * Generates possibly pending bytes & trailers to complete the HTTP message.
   */
  void generateTrailer(const HeaderFieldList& trailers);

  /**
   * Retrieves the number of bytes remaining for the content.
   */
  size_t remainingContentLength() const noexcept {
    return contentLength() - actualContentLength();
  }

  size_t contentLength() const noexcept { return contentLength_; }
  size_t actualContentLength() const noexcept { return actualContentLength_; }

  /**
   * Retrieves boolean indicating whether chunked response is generated.
   */
  bool isChunked() const noexcept { return chunked_; }

  size_t bytesTransmitted() const noexcept { return bytesTransmitted_; }

 private:
  void generateRequestLine(const HttpRequestInfo& info);
  void generateResponseLine(const HttpResponseInfo& info);
  void generateHeaders(const HttpInfo& info, bool bodyForbidden = false);
  void generateResponseInfo(const HttpResponseInfo& info);
  void flushBuffer();

 private:
  size_t bytesTransmitted_;
  size_t contentLength_;
  size_t actualContentLength_;
  bool chunked_;
  Buffer buffer_;
  EndPointWriter* writer_;
};

}  // namespace http1
}  // namespace http
}  // namespace xzero
