// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/UnixTime.h>
#include <xzero/Duration.h>
#include <xzero/io/File.h>
#include <xzero/CustomDataMgr.h>
#include <xzero/net/IPAddress.h>
#include <xzero/logging.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/http/HttpRequest.h>
#include <flow/vm/Runner.h>
#include <string>
#include <list>
#include <vector>
#include <functional>
#include <memory>

namespace xzero {
  class UnixTime;

  namespace http {
    class HttpRequest;
    class HttpResponse;
  }
}

namespace x0d {

/**
 * HTTP client context.
 *
 * Contains all the necessary references to everything you (may) need
 * during request handling.
 */
class Context {
  CUSTOMDATA_API_INLINE
 public:
  Context(
      const flow::Handler* requestHandler,
      xzero::http::HttpRequest* request,
      xzero::http::HttpResponse* response,
      const std::unordered_map<xzero::http::HttpStatus, std::string>* globalErrorPages,
      size_t maxInternalRedirectCount);

  Context(Context&&) = default;
  Context& operator=(Context&&) = default;

  Context(const Context&);
  Context& operator=(const Context&) = delete;

  ~Context();

  void operator()();
  void handleRequest();

  /**
   * Tries to serve a TRACE request.
   */
  bool tryServeTraceProxy();

  /**
   * Tries to serve a TRACE request as the ORIGIN server.
   */
  bool tryServeTraceOrigin();

  /**
   * Actually serves TRACE response without checking request method or headers.
   */
  void serveTraceOrigin();

  xzero::http::HttpRequest* masterRequest() const noexcept { return request_; }

  xzero::http::HttpRequest* request() const noexcept {
    return internalRedirects_.empty()
        ? masterRequest()
        : internalRedirects_.front().get();
  }

  xzero::http::HttpResponse* response() const noexcept { return response_; }

  size_t internalRedirectCount() const noexcept { return internalRedirects_.size(); }

  xzero::UnixTime createdAt() const noexcept { return createdAt_; }
  xzero::UnixTime now() const noexcept;
  xzero::Duration age() const noexcept;

  const std::string& documentRoot() const noexcept { return documentRoot_; }
  void setDocumentRoot(const std::string& path) { documentRoot_ = path; }

  const std::string& pathInfo() const noexcept { return pathInfo_; }
  void setPathInfo(const std::string& value) { pathInfo_ = value; }

  void setFile(std::shared_ptr<xzero::File> file) { file_ = file; }
  std::shared_ptr<xzero::File> file() const { return file_; }

  flow::Runner* runner() const noexcept { return runner_.get(); }

  const xzero::IPAddress& remoteIP() const;
  int remotePort() const;

  const xzero::IPAddress& localIP() const;
  int localPort() const;

  size_t bytesReceived() const noexcept;
  size_t bytesTransmitted() const noexcept;

  void setErrorPage(xzero::http::HttpStatus status, const std::string& uri);
  bool getErrorPage(xzero::http::HttpStatus status, std::string* uri) const;

  /**
   * Sends an error page via an internal redirect or by generating a basic response.
   *
   * @param status the HTTP status code to send to the client.
   * @param internalRedirect output set to @c true if the result is an internal
   *                         redirect, false (HTTP response fully generated)
   *                         otherwise.
   * @param overrideStatus status to actually send to the client (may differ
   *                       from the status to match the error page)
   *
   * @retval true a response was generated
   * @retval false no response was generated but an internal redirect was triggered.
   *
   * It is important to note, that this call either fully generates
   * a response and no further handling has to be done, or
   * an internal redirect was triggered and the request handler has to be
   * resumed for execution.
   */
  bool sendErrorPage(
      xzero::http::HttpStatus status,
      bool* internalRedirect = nullptr,
      xzero::http::HttpStatus overrideStatus = xzero::http::HttpStatus::Undefined);

  /**
   * Sends a trivial response, with simple content if content not forbidden.
   *
   * A trivial response is having set the HTTP response status code with
   * (if allowed) static content as descriptive content.
   *
   * @param status HTTP status to send
   * @param reason reason associated with that status; the text version of the
   *               HTTP status will be used if this string is empty.
   */
  void sendTrivialResponse(xzero::http::HttpStatus status, const std::string& reason = std::string());

  // {{{ Logging API
  template<typename... Args>
  inline void logError(const std::string& fmt, Args&&... args) {
    ::xzero::logError(::fmt::format("{}: {}", masterRequest()->remoteAddress(), fmt), args...);
  }

  template<typename... Args>
  inline void logWarning(const std::string& fmt, Args&&... args) {
    ::xzero::logWarning(::fmt::format("{}: {}", masterRequest()->remoteAddress(), fmt), args...);
  }

  template<typename... Args>
  inline void logNotice(const std::string& fmt, Args&&... args) {
    ::xzero::logNotice(::fmt::format("{}: {}", masterRequest()->remoteAddress(), fmt), args...);
  }

  template<typename... Args>
  inline void logInfo(const std::string& fmt, Args&&... args) {
    ::xzero::logInfo(::fmt::format("{}: {}", masterRequest()->remoteAddress(), fmt), args...);
  }

  template<typename... Args>
  inline void logDebug(const std::string& fmt, Args&&... args) {
    ::xzero::logDebug(::fmt::format("{}: {}", masterRequest()->remoteAddress(), fmt), args...);
  }
  // }}}

 private:
  const flow::Handler* requestHandler_; //!< HTTP request handler as flow program
  std::unique_ptr<flow::Runner> runner_; //!< Flow VM execution unit.
  const xzero::UnixTime createdAt_; //!< When the request started
  xzero::http::HttpRequest* request_; //!< actual HTTP request
  std::list<std::unique_ptr<xzero::http::HttpRequest>> internalRedirects_;
  xzero::http::HttpResponse* response_; //!< HTTP response
  std::string documentRoot_; //!< associated document root
  std::string pathInfo_; //!< info-part of the request-path
  std::shared_ptr<xzero::File> file_; //!< local file associated with this request
  std::unordered_map<xzero::http::HttpStatus, std::string> errorPages_; //!< custom error page request paths
  const std::unordered_map<xzero::http::HttpStatus, std::string>* globalErrorPages_;
  const size_t maxInternalRedirectCount_;
};

} // namespace x0d
