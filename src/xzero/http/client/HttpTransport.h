// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/CompletionHandler.h>

namespace xzero {

class FileView;

namespace http {

class HttpRequestInfo;
class HttpListener;

namespace client {

/**
 * HTTP client-side Transport Layer API
 */
class HttpTransport {
 public:
  virtual ~HttpTransport();

  virtual void setListener(HttpListener* channel) = 0;
  virtual void send(const HttpRequestInfo& requestInfo,
                    CompletionHandler onComplete) = 0;
  virtual void send(const HttpRequestInfo& requestInfo,
                    const BufferRef& chunk,
                    CompletionHandler onComplete) = 0;
  virtual void send(const HttpRequestInfo& requestInfo,
                    Buffer&& chunk,
                    CompletionHandler onComplete) = 0;
  virtual void send(const HttpRequestInfo& requestInfo,
                    FileView&& chunk,
                    CompletionHandler onComplete) = 0;
  virtual void send(const BufferRef& chunk, CompletionHandler onComplete) = 0;
  virtual void send(Buffer&& chunk, CompletionHandler onComplete) = 0;
  virtual void send(FileView&& chunk, CompletionHandler onComplete) = 0;
  virtual void completed() = 0;
  virtual void abort() = 0;
};

} // namespace client
} // namespace http
} // namespace xzero
