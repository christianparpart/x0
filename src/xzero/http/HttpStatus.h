// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <system_error>
#include <string>

namespace xzero {
namespace http {

//! \addtogroup http
//@{

/**
 * HTTP status code.
 *
 * \see http://www.iana.org/assignments/http-status-codes/http-status-codes.xml
 */
enum class HttpStatus  // {{{
{
  Undefined = 0,

  // informational
  ContinueRequest = 100,
  SwitchingProtocols = 101,
  Processing = 102,  // WebDAV, RFC 2518

  // successful
  Ok = 200,
  Created = 201,
  Accepted = 202,
  NonAuthoriativeInformation = 203,
  NoContent = 204,
  ResetContent = 205,
  PartialContent = 206,

  // redirection
  MultipleChoices = 300,
  MovedPermanently = 301,
  Found = 302,
  MovedTemporarily = Found,
  NotModified = 304,
  TemporaryRedirect = 307,  // since HTTP/1.1
  PermanentRedirect = 308,  // Internet-Draft

  // client error
  BadRequest = 400,
  Unauthorized = 401,
  PaymentRequired = 402,  // reserved for future use
  Forbidden = 403,
  NotFound = 404,
  MethodNotAllowed = 405,
  NotAcceptable = 406,
  ProxyAuthenticationRequired = 407,
  RequestTimeout = 408,
  Conflict = 409,
  Gone = 410,
  LengthRequired = 411,
  PreconditionFailed = 412,
  PayloadTooLarge = 413,
  RequestUriTooLong = 414,
  UnsupportedMediaType = 415,
  RequestedRangeNotSatisfiable = 416,
  ExpectationFailed = 417,
  MisdirectedRequest = 421,
  UnprocessableEntity = 422,
  Locked = 423,
  FailedDependency = 424,
  UnorderedCollection = 425,
  UpgradeRequired = 426,
  PreconditionRequired = 428,         // RFC 6585
  TooManyRequests = 429,              // RFC 6585
  RequestHeaderFieldsTooLarge = 431,  // RFC 6585
  NoResponse = 444,  // nginx ("Used in Nginx logs to indicate that the server
                     // has returned no information to the client and closed the
                     // connection")
  Hangup = 499,  // Used in Nginx to indicate that the client has aborted the
                 // connection before the server could serve the response.

  // server error
  InternalServerError = 500,
  NotImplemented = 501,
  BadGateway = 502,
  ServiceUnavailable = 503,
  GatewayTimeout = 504,
  HttpVersionNotSupported = 505,
  VariantAlsoNegotiates = 506,         // RFC 2295
  InsufficientStorage = 507,           // WebDAV, RFC 4918
  LoopDetected = 508,                  // WebDAV, RFC 5842
  BandwidthExceeded = 509,             // Apache
  NotExtended = 510,                   // RFC 2774
  NetworkAuthenticationRequired = 511  // RFC 6585
};
// }}}

/**
 * HttpStatusGroup classifies HttpStatus codes into groups.
 */
enum class HttpStatusGroup {
  Informational = 1,
  Success = 2,
  Redirect = 3,
  ClientError = 4,
  ServerError = 5,
};

class XZERO_HTTP_API HttpStatusCategory : public std::error_category {
 public:
  static std::error_category& get();

  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

constexpr bool operator!(HttpStatus st) {
  return st == HttpStatus::Undefined;
}

/** Classifies an HttpStatus by converting to HttpStatusGroup. */
constexpr HttpStatusGroup toStatusGroup(HttpStatus status) {
  return static_cast<HttpStatusGroup>(static_cast<int>(status) / 100);
}

/** Retrieves the human readable text of the HTTP status @p code. */
XZERO_HTTP_API const std::string& to_string(HttpStatus code);

/** Tests whether given status @p code MUST NOT have a message body. */
constexpr bool isContentForbidden(HttpStatus code);

/** Tests whether given status @p code is informatiional (1xx). */
constexpr bool isInformational(HttpStatus code);

/** Tests whether given status @p code is successful (2xx). */
constexpr bool isSuccess(HttpStatus code);

/** Tests whether given status @p code is a redirect (3xx). */
constexpr bool isRedirect(HttpStatus code);

/** Tests whether given status @p code is a client or server error (4xx, 5xx). */
constexpr bool isError(HttpStatus code);

/** Tests whether given status @p code is a client error (4xx). */
constexpr bool isClientError(HttpStatus code);

/** Tests whether given status @p code is a server error (5xx). */
constexpr bool isServerError(HttpStatus code);
//@}

// {{{ constexpr's / inlines
constexpr bool isInformational(HttpStatus code) {
  return toStatusGroup(code) == HttpStatusGroup::Informational;
}

constexpr bool isSuccess(HttpStatus code) {
  return toStatusGroup(code) == HttpStatusGroup::Success;
}

constexpr bool isRedirect(HttpStatus code) {
  return toStatusGroup(code) == HttpStatusGroup::Redirect;
}

constexpr bool isError(HttpStatus code) {
  return isClientError(code) || isServerError(code);
}

constexpr bool isClientError(HttpStatus code) {
  return toStatusGroup(code) == HttpStatusGroup::ClientError;
}

constexpr bool isServerError(HttpStatus code) {
  return toStatusGroup(code) == HttpStatusGroup::ServerError;
}

constexpr bool isContentForbidden(HttpStatus code) {
  switch (code) {
    case /*100*/ HttpStatus::ContinueRequest:
    case /*101*/ HttpStatus::SwitchingProtocols:
    case /*204*/ HttpStatus::NoContent:
    case /*205*/ HttpStatus::ResetContent:
    case /*304*/ HttpStatus::NotModified:
    case /*444*/ HttpStatus::NoResponse:
    case /*499*/ HttpStatus::Hangup:
      return true;
    default:
      return false;
  }
}
// }}}

inline std::error_code make_error_code(HttpStatus status) {
  return std::error_code((int) status, HttpStatusCategory::get());
}

}  // namespace http
}  // namespace xzero

namespace std {

template <>
struct hash<xzero::http::HttpStatus> : public unary_function<xzero::http::HttpStatus, size_t> {
  size_t operator()(xzero::http::HttpStatus status) const {
    return (size_t) status;
  }
};

template<> struct is_error_code_enum<xzero::http::HttpStatus> : public true_type{};

}  // namespace std
