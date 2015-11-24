// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Buffer.h>
#include <xzero/CompletionHandler.h>

namespace xzero {

class FileRef;

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
                    FileRef&& chunk,
                    CompletionHandler onComplete) = 0;
  virtual void send(const BufferRef& chunk, CompletionHandler onComplete) = 0;
  virtual void send(Buffer&& chunk, CompletionHandler onComplete) = 0;
  virtual void send(FileRef&& chunk, CompletionHandler onComplete) = 0;
  virtual void completed() = 0;
  virtual void abort() = 0;
};

} // namespace client
} // namespace http
} // namespace xzero
