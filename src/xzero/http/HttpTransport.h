// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/net/Connection.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Buffer.h>
#include <memory>

namespace xzero {

class FileView;

namespace http {

class HttpResponseInfo;

/**
 * HTTP transport layer interface.
 *
 * Implemements the wire transport protocol for HTTP messages without
 * any semantics.
 *
 * For HTTP/1 for example it is <b>RFC 7230</b>.
 */
class XZERO_HTTP_API HttpTransport {
 public:
  virtual ~HttpTransport();

  /**
   * Cancels communication completely.
   */
  virtual void abort() = 0;

  /**
   * Invoked when the current message has been fully generated.
   *
   * This does not imply transmission, there can still be some bytes left.
   */
  virtual void completed() = 0;

  /**
   * Initiates sending a response to the client.
   *
   * @param responseInfo HTTP response meta informations.
   * @param chunk response body data chunk.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   *
   * @note You must ensure the data chunk is available until sending completed!
   */
  virtual void send(HttpResponseInfo& responseInfo, const BufferRef& chunk,
                    CompletionHandler onComplete) = 0;

  /**
   * Initiates sending a response to the client.
   *
   * @param responseInfo HTTP response meta informations.
   * @param chunk response body data chunk.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(HttpResponseInfo& responseInfo, Buffer&& chunk,
                    CompletionHandler onComplete) = 0;

  /**
   * Initiates sending a response to the client.
   *
   * @param responseInfo HTTP response meta informations.
   * @param chunk response body chunk represented as a file.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(HttpResponseInfo& responseInfo, FileView&& chunk,
                    CompletionHandler onComplete) = 0;

  /**
   * Transfers this data chunk to the output stream.
   *
   * @param chunk response body chunk
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(Buffer&& chunk, CompletionHandler onComplete) = 0;

  /**
   * Transfers this file data chunk to the output stream.
   *
   * @param chunk response body chunk represented as a file.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(FileView&& chunk, CompletionHandler onComplete) = 0;

  /**
   * Transfers this data chunk to the output stream.
   *
   * @param chunk response body chunk
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  virtual void send(const BufferRef& chunk, CompletionHandler onComplete) = 0;
};

}  // namespace http
}  // namespace xzero
