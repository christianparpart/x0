// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/HttpStatus.h>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace xzero {

class File;
class MimeTypes;

namespace http {

class HttpRequest;
class HttpResponse;

/**
 * Handles GET/HEAD requests to local files.
 *
 * @note this class is not meant to be thread safe.
 */
class XZERO_HTTP_API HttpFileHandler {
 public:
  /**
   * Initializes static file handler.
   */
  explicit HttpFileHandler();

  /**
   * Initializes static file handler.
   *
   * @param generateBoundaryID boundary-ID generator function that generates
   *                           response-local unique boundary IDs.
   */
  HttpFileHandler(std::function<std::string()> generateBoundaryID);

  ~HttpFileHandler();

  /**
   * Handles given @p request if a local file @p transferFile exists.
   *
   * Iff the given request was successfully handled, the response is
   * being also marked as completed, and thus, any future call to
   * the request or response object will be invalid.
   *
   * @param request the request to handle.
   * @param response the response to generate.
   *
   * @param HttpStatus::Ok Full document is being sent. The actual HTTP status
   * code may differ due to internal redirects, but this return code declares
   * this request as being fully handled.
   * @param HttpStatus::PartialContent Partial content (ranged-request) sent.
   * @param HttpStatus::NotModified Client side cache was hit. No response was generated.
   * @param HttpStatus::PreconditionFailed Client's precondition failed. No response was generated.
   * @param HttpStatus::NotFound HTTP request not handled, most probably
   * because the underlying file was not found or is not a file.
   * No response was generated.
   * @param HttpStatus::MethodNotAllowed Unsupported method detected. No response was generated.
   */
  HttpStatus handle(HttpRequest* request,
                    HttpResponse* response,
                    std::shared_ptr<File> transferFile);

 private:
  /**
   * Evaluates conditional requests to local file.
   *
   * @param transferFile the targeted file that is meant to be transferred.
   * @param request HTTP request handle.
   * @param response HTTP response handle.
   *
   * @retval HttpStatus::NotModified if client cache is valid
   * @retval HttpStatus::Precondition HTTP client's precondition failed.
   * @retval HttpStatus::Undefined if client cache is invalid or inexistent.
   *
   * This method tests whether the @p request is conditional.
   * It checks for the presense of request header fields, such as
   * "If-Match", "If-None-Match", "If-Modified-Since", "If-Unmodified-Since",
   * and if found and evaluated to be true, the request will
   * be served directly with a "Not Modified" or "Precondition Failed".
   *
   * If the conditionas fail then no operations has been made to the @p response.
   */
  HttpStatus handleClientCache(const File& transferFile,
                               HttpRequest* request,
                               HttpResponse* response);

  /**
   * Fully processes the ranged requests, if one, or does nothing.
   *
   * @param transferFile
   * @param fd open file descriptor in case of a GET request.
   * @param request HTTP request handle.
   * @param response HTTP response handle.
   *
   * @retval true this was a ranged request and we fully processed it, e.g.
   *              HttpResponse:::completed() was invoked.
   * @retval false this is no ranged request.
   *
   * @note if this is no ranged request then nothing is done on it.
   */
  bool handleRangeRequest(const File& transferFile, int fd,
                          HttpRequest* request, HttpResponse* response);

 private:
  std::function<std::string()> generateBoundaryID_;

  // TODO stat cache
  // TODO fd cache
};

} // namespace http
} // namespace xzero
