// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/http/HttpOutput.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/sysconfig.h>
#include <memory>

namespace xzero {

class Executor;

namespace http {

class HttpChannel;
class HttpOutput;

/**
 * Represents an HTTP response message.
 *
 * Semantic HTTP-protocol headers, such as "Date" will must not be added
 * as they are added by the HttpGenerator when flushing the response
 * to the client.
 *
 * @note It is not safe to mutate a response from multiple threads concurrently.
 */
class XZERO_HTTP_API HttpResponse {
 private:
  HttpResponse(HttpResponse&) = delete;
  HttpResponse& operator=(HttpResponse&) = delete;

 public:
  HttpResponse(HttpChannel* channel, std::unique_ptr<HttpOutput>&& output);

  Executor* executor() const noexcept;

  void recycle();

  HttpVersion version() const XZERO_NOEXCEPT;
  void setVersion(HttpVersion version);

  void setStatus(HttpStatus status);
  HttpStatus status() const XZERO_NOEXCEPT { return status_; }
  bool hasStatus() const XZERO_NOEXCEPT { return status_ != HttpStatus::Undefined; }

  const std::string& reason() const XZERO_NOEXCEPT { return reason_; }
  void setReason(const std::string& val);

  // high level header support
  void resetContentLength();
  void setContentLength(size_t size);

  size_t contentLength() const XZERO_NOEXCEPT {
    return contentLength_;
  }

  bool hasContentLength() const XZERO_NOEXCEPT {
    return contentLength_ != static_cast<size_t>(-1);
  }

  // headers
  void addHeader(const std::string& name, const std::string& value);
  void appendHeader(const std::string& name, const std::string& value,
                    const std::string& delim = "");
  void setHeader(const std::string& name, const std::string& value);
  void removeHeader(const std::string& name);
  void removeAllHeaders();
  const std::string& getHeader(const std::string& name) const;
  const HeaderFieldList& headers() const XZERO_NOEXCEPT { return headers_; }
  HeaderFieldList& headers() XZERO_NOEXCEPT { return headers_; }

  // trailers
  //bool isTrailerSupported() const;
  void registerTrailer(const std::string& name);
  void appendTrailer(const std::string& name, const std::string& value,
                    const std::string& delim = "");
  void setTrailer(const std::string& name, const std::string& value);
  const HeaderFieldList& trailers() const XZERO_NOEXCEPT { return trailers_; }

  /**
   * Installs a callback to be invoked right before serialization of response
   * headers.
   */
  void onPostProcess(std::function<void()> callback);

  /**
   * Installs a callback to be invoked right after the last response message
   * byte has been fully sent or transmission has been aborted.
   */
  void onResponseEnd(std::function<void()> callback);

  /**
   * Invoke to mark this response as complete.
   *
   * Further access to this object is undefined.
   */
  void completed();

  /**
   * Invoke to tell the client that it may continue sending the request body.
   *
   * You may only invoke this method if and only if the client actually
   * requested this behaviour via <code>Expect: 100-continue</code>
   * request header.
   */
  void send100Continue(CompletionHandler onComplete);

  /**
   * Responds with an error response message.
   *
   * @param code HTTP response status code.
   * @param message optional custom error message.
   *
   * @note This message is considered completed after this call.
   */
  void sendError(HttpStatus code, const std::string& message = "");

  HttpOutput* output() { return output_.get(); }

  bool isCommitted() const XZERO_NOEXCEPT { return committed_; }

  void setBytesTransmitted(size_t n) { bytesTransmitted_ = n; }
  size_t bytesTransmitted() const noexcept { return bytesTransmitted_; }

 private:
  friend class HttpChannel;
  void setCommitted(bool value);
  /** ensures response wasn't committed yet and thus has mutabale meta info. */
  void requireMutableInfo();
  /** ensures that channel is not in sending state. */
  void requireNotSendingAlready();

 private:
  HttpChannel* channel_;
  std::unique_ptr<HttpOutput> output_;

  HttpVersion version_;
  HttpStatus status_;
  std::string reason_;
  size_t contentLength_;
  HeaderFieldList headers_;
  HeaderFieldList trailers_;
  bool committed_;
  size_t bytesTransmitted_;
};

}  // namespace http
}  // namespace xzero
