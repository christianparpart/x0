// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
#include <xzero/http/HttpStatus.h>
#include <xzero-flow/vm/Params.h>
#include <xzero-flow/vm/Runner.h>
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
class XzeroContext {
  CUSTOMDATA_API_INLINE
 public:
  XzeroContext(
      std::shared_ptr<xzero::flow::vm::Handler> entrypoint,
      xzero::http::HttpRequest* request,
      xzero::http::HttpResponse* response,
      std::unordered_map<xzero::http::HttpStatus, std::string>* globalErrorPages,
      size_t maxInternalRedirectCount);
  ~XzeroContext();

  xzero::http::HttpRequest* masterRequest() const noexcept { return requests_.back(); }
  xzero::http::HttpRequest* request() const noexcept { return requests_.front(); }
  xzero::http::HttpResponse* response() const noexcept { return response_; }

  size_t internalRedirectCount() const { return requests_.size() - 1; }

  xzero::UnixTime createdAt() const { return createdAt_; }
  xzero::UnixTime now() const;
  xzero::Duration age() const;

  const std::string& documentRoot() const noexcept { return documentRoot_; }
  void setDocumentRoot(const std::string& path) { documentRoot_ = path; }

  const std::string& pathInfo() const noexcept { return pathInfo_; }
  void setPathInfo(const std::string& value) { pathInfo_ = value; }

  void setFile(std::shared_ptr<xzero::File> file) { file_ = file; }
  std::shared_ptr<xzero::File> file() const { return file_; }

  xzero::flow::vm::Runner* runner() const noexcept { return runner_.get(); }

  const xzero::IPAddress& remoteIP() const;
  int remotePort() const;

  const xzero::IPAddress& localIP() const;
  int localPort() const;

  size_t bytesReceived() const;
  size_t bytesTransmitted() const;

  bool verifyDirectoryDepth();

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
   * It is important to note, that this call either fully generates
   * a response and no further handling has to be done, or
   * an internal redirect was triggered and the request handler has to be
   * resumed for execution.
   */
  void sendErrorPage(
      xzero::http::HttpStatus status,
      bool* internalRedirect,
      xzero::http::HttpStatus overrideStatus = xzero::http::HttpStatus::Undefined);

  /**
   * Sends a status page with simple content (if not forbidden).
   *
   * @param status HTTP status to send
   * @param reason reason associated with that status; the text version of the
   *               HTTP status will be used if this string is empty.
   */
  void sendSimpleStatusPage(xzero::http::HttpStatus status, const std::string& reason = std::string());

 private:
  std::unique_ptr<xzero::flow::vm::Runner> runner_; //!< Flow VM execution unit.
  const xzero::UnixTime createdAt_; //!< When the request started
  std::list<xzero::http::HttpRequest*> requests_; //!< HTTP request
  xzero::http::HttpResponse* response_; //!< HTTP response
  std::string documentRoot_; //!< associated document root
  std::string pathInfo_; //!< info-part of the request-path
  std::shared_ptr<xzero::File> file_; //!< local file associated with this request
  std::unordered_map<xzero::http::HttpStatus, std::string> errorPages_; //!< custom error page request paths
  const std::unordered_map<xzero::http::HttpStatus, std::string>* globalErrorPages_;
  const size_t maxInternalRedirectCount_;
};

} // namespace x0d
