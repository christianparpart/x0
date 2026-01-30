// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <format>
#include <iosfwd>
#include <string>
#include <system_error>
#include <xzero/defines.h>

namespace xzero::http {

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

class HttpStatusCategory : public std::error_category {
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
const std::string& as_string(HttpStatus code);

/** Tests whether given status @p code MUST NOT have a message body. */
constexpr bool isContentForbidden(HttpStatus code);

/** Tests whether given status @p code is informatiional (1xx). */
constexpr bool isInformational(HttpStatus code);

/** Tests whether given status @p code is successful (2xx). */
constexpr bool isSuccess(HttpStatus code);

/** Tests whether given status @p code is a redirect (3xx). */
constexpr bool isRedirect(HttpStatus code);

/** Tests whether given status @p code is a client or server error (4xx, 5xx).
 */
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
  return std::error_code((int)status, HttpStatusCategory::get());
}

}  // namespace xzero::http

namespace std {
template <>
struct hash<xzero::http::HttpStatus> {
  constexpr size_t operator()(xzero::http::HttpStatus status) const {
    return static_cast<size_t>(status);
  }
};

template <>
struct is_error_code_enum<xzero::http::HttpStatus> : public true_type {};
}  // namespace std

template <>
struct std::formatter<xzero::http::HttpStatus> {
  using HttpStatus = xzero::http::HttpStatus;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const HttpStatus& v, std::format_context& ctx) const {
    switch (v) {
      case HttpStatus::ContinueRequest:
        return std::format_to(ctx.out(), "Continue Request");
      case HttpStatus::SwitchingProtocols:
        return std::format_to(ctx.out(), "Switching Protocols");
      case HttpStatus::Processing:
        return std::format_to(ctx.out(), "Processing");
      case HttpStatus::Ok:
        return std::format_to(ctx.out(), "Ok");
      case HttpStatus::Created:
        return std::format_to(ctx.out(), "Created");
      case HttpStatus::Accepted:
        return std::format_to(ctx.out(), "Accepted");
      case HttpStatus::NonAuthoriativeInformation:
        return std::format_to(ctx.out(), "Non Authoriative Information");
      case HttpStatus::NoContent:
        return std::format_to(ctx.out(), "No Content");
      case HttpStatus::ResetContent:
        return std::format_to(ctx.out(), "Reset Content");
      case HttpStatus::PartialContent:
        return std::format_to(ctx.out(), "Partial Content");
      case HttpStatus::MultipleChoices:
        return std::format_to(ctx.out(), "Multiple Choices");
      case HttpStatus::MovedPermanently:
        return std::format_to(ctx.out(), "Moved Permanently");
      case HttpStatus::Found:
        return std::format_to(ctx.out(), "Found");
      case HttpStatus::NotModified:
        return std::format_to(ctx.out(), "Not Modified");
      case HttpStatus::TemporaryRedirect:
        return std::format_to(ctx.out(), "Temporary Redirect");
      case HttpStatus::PermanentRedirect:
        return std::format_to(ctx.out(), "Permanent Redirect");
      case HttpStatus::BadRequest:
        return std::format_to(ctx.out(), "Bad Request");
      case HttpStatus::Unauthorized:
        return std::format_to(ctx.out(), "Unauthorized");
      case HttpStatus::PaymentRequired:
        return std::format_to(ctx.out(), "Payment Required");
      case HttpStatus::Forbidden:
        return std::format_to(ctx.out(), "Forbidden");
      case HttpStatus::NotFound:
        return std::format_to(ctx.out(), "Not Found");
      case HttpStatus::MethodNotAllowed:
        return std::format_to(ctx.out(), "Method Not Allowed");
      case HttpStatus::NotAcceptable:
        return std::format_to(ctx.out(), "Not Acceptable");
      case HttpStatus::ProxyAuthenticationRequired:
        return std::format_to(ctx.out(), "Proxy Authentication Required");
      case HttpStatus::RequestTimeout:
        return std::format_to(ctx.out(), "Request Timeout");
      case HttpStatus::Conflict:
        return std::format_to(ctx.out(), "Conflict");
      case HttpStatus::Gone:
        return std::format_to(ctx.out(), "Gone");
      case HttpStatus::LengthRequired:
        return std::format_to(ctx.out(), "Length Required");
      case HttpStatus::PreconditionFailed:
        return std::format_to(ctx.out(), "Precondition Failed");
      case HttpStatus::PayloadTooLarge:
        return std::format_to(ctx.out(), "Payload Too Large");
      case HttpStatus::RequestUriTooLong:
        return std::format_to(ctx.out(), "Request Uri Too Long");
      case HttpStatus::UnsupportedMediaType:
        return std::format_to(ctx.out(), "Unsupported Media Type");
      case HttpStatus::RequestedRangeNotSatisfiable:
        return std::format_to(ctx.out(), "Requested Range Not Satisfiable");
      case HttpStatus::ExpectationFailed:
        return std::format_to(ctx.out(), "Expectation Failed");
      case HttpStatus::MisdirectedRequest:
        return std::format_to(ctx.out(), "Misdirected Request");
      case HttpStatus::UnprocessableEntity:
        return std::format_to(ctx.out(), "Unprocessable Entity");
      case HttpStatus::Locked:
        return std::format_to(ctx.out(), "Locked");
      case HttpStatus::FailedDependency:
        return std::format_to(ctx.out(), "Failed Dependency");
      case HttpStatus::UnorderedCollection:
        return std::format_to(ctx.out(), "Unordered Collection");
      case HttpStatus::UpgradeRequired:
        return std::format_to(ctx.out(), "Upgrade Required");
      case HttpStatus::PreconditionRequired:
        return std::format_to(ctx.out(), "Precondition Required");
      case HttpStatus::TooManyRequests:
        return std::format_to(ctx.out(), "Too Many Requests");
      case HttpStatus::RequestHeaderFieldsTooLarge:
        return std::format_to(ctx.out(), "Request Header Fields Too Large");
      case HttpStatus::InternalServerError:
        return std::format_to(ctx.out(), "Internal Server Error");
      case HttpStatus::NotImplemented:
        return std::format_to(ctx.out(), "Not Implemented");
      case HttpStatus::BadGateway:
        return std::format_to(ctx.out(), "Bad Gateway");
      case HttpStatus::ServiceUnavailable:
        return std::format_to(ctx.out(), "Service Unavailable");
      case HttpStatus::GatewayTimeout:
        return std::format_to(ctx.out(), "Gateway Timeout");
      case HttpStatus::HttpVersionNotSupported:
        return std::format_to(ctx.out(), "Http Version Not Supported");
      case HttpStatus::VariantAlsoNegotiates:
        return std::format_to(ctx.out(), "Variant Also Negotiates");
      case HttpStatus::InsufficientStorage:
        return std::format_to(ctx.out(), "Insufficient Storage");
      case HttpStatus::LoopDetected:
        return std::format_to(ctx.out(), "Loop Detected");
      case HttpStatus::BandwidthExceeded:
        return std::format_to(ctx.out(), "Bandwidth Exceeded");
      case HttpStatus::NotExtended:
        return std::format_to(ctx.out(), "Not Extended");
      case HttpStatus::NetworkAuthenticationRequired:
        return std::format_to(ctx.out(), "Network Authentication Required");
      default:
        return std::format_to(ctx.out(), "({})", (int)v);
    }
  }
};
