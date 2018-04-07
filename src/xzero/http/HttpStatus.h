// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <fmt/format.h>
#include <system_error>
#include <string>
#include <iosfwd>

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

}  // namespace xzero::http

namespace std {
#if defined(XZERO_OS_WIN32)
  template<>
  struct hash<xzero::http::HttpStatus> {
    constexpr size_t operator()(xzero::http::HttpStatus status) const {
      return static_cast<size_t>(status);
    }
  };
#else
  template <>
  struct hash<xzero::http::HttpStatus> : public unary_function<xzero::http::HttpStatus, size_t> {
    size_t operator()(xzero::http::HttpStatus status) const {
      return (size_t) status;
    }
  };
#endif

  template<> struct is_error_code_enum<xzero::http::HttpStatus> : public true_type {};
}  // namespace std

namespace fmt {
  template<>
  struct formatter<xzero::http::HttpStatus> {
    using HttpStatus = xzero::http::HttpStatus;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const HttpStatus& v, FormatContext &ctx) {
      switch (v) {
        case HttpStatus::ContinueRequest: return format_to(ctx.begin(), "Continue Request");
        case HttpStatus::SwitchingProtocols: return format_to(ctx.begin(), "Switching Protocols");
        case HttpStatus::Processing: return format_to(ctx.begin(), "Processing");
        case HttpStatus::Ok: return format_to(ctx.begin(), "Ok");
        case HttpStatus::Created: return format_to(ctx.begin(), "Created");
        case HttpStatus::Accepted: return format_to(ctx.begin(), "Accepted");
        case HttpStatus::NonAuthoriativeInformation: return format_to(ctx.begin(), "Non Authoriative Information");
        case HttpStatus::NoContent: return format_to(ctx.begin(), "No Content");
        case HttpStatus::ResetContent: return format_to(ctx.begin(), "Reset Content");
        case HttpStatus::PartialContent: return format_to(ctx.begin(), "Partial Content");
        case HttpStatus::MultipleChoices: return format_to(ctx.begin(), "Multiple Choices");
        case HttpStatus::MovedPermanently: return format_to(ctx.begin(), "Moved Permanently");
        case HttpStatus::Found: return format_to(ctx.begin(), "Found");
        case HttpStatus::NotModified: return format_to(ctx.begin(), "Not Modified");
        case HttpStatus::TemporaryRedirect: return format_to(ctx.begin(), "Temporary Redirect");
        case HttpStatus::PermanentRedirect: return format_to(ctx.begin(), "Permanent Redirect");
        case HttpStatus::BadRequest: return format_to(ctx.begin(), "Bad Request");
        case HttpStatus::Unauthorized: return format_to(ctx.begin(), "Unauthorized");
        case HttpStatus::PaymentRequired: return format_to(ctx.begin(), "Payment Required");
        case HttpStatus::Forbidden: return format_to(ctx.begin(), "Forbidden");
        case HttpStatus::NotFound: return format_to(ctx.begin(), "Not Found");
        case HttpStatus::MethodNotAllowed: return format_to(ctx.begin(), "Method Not Allowed");
        case HttpStatus::NotAcceptable: return format_to(ctx.begin(), "Not Acceptable");
        case HttpStatus::ProxyAuthenticationRequired: return format_to(ctx.begin(), "Proxy Authentication Required");
        case HttpStatus::RequestTimeout: return format_to(ctx.begin(), "Request Timeout");
        case HttpStatus::Conflict: return format_to(ctx.begin(), "Conflict");
        case HttpStatus::Gone: return format_to(ctx.begin(), "Gone");
        case HttpStatus::LengthRequired: return format_to(ctx.begin(), "Length Required");
        case HttpStatus::PreconditionFailed: return format_to(ctx.begin(), "Precondition Failed");
        case HttpStatus::PayloadTooLarge: return format_to(ctx.begin(), "Payload Too Large");
        case HttpStatus::RequestUriTooLong: return format_to(ctx.begin(), "Request Uri Too Long");
        case HttpStatus::UnsupportedMediaType: return format_to(ctx.begin(), "Unsupported Media Type");
        case HttpStatus::RequestedRangeNotSatisfiable: return format_to(ctx.begin(), "Requested Range Not Satisfiable");
        case HttpStatus::ExpectationFailed: return format_to(ctx.begin(), "Expectation Failed");
        case HttpStatus::MisdirectedRequest: return format_to(ctx.begin(), "Misdirected Request");
        case HttpStatus::UnprocessableEntity: return format_to(ctx.begin(), "Unprocessable Entity");
        case HttpStatus::Locked: return format_to(ctx.begin(), "Locked");
        case HttpStatus::FailedDependency: return format_to(ctx.begin(), "Failed Dependency");
        case HttpStatus::UnorderedCollection: return format_to(ctx.begin(), "Unordered Collection");
        case HttpStatus::UpgradeRequired: return format_to(ctx.begin(), "Upgrade Required");
        case HttpStatus::PreconditionRequired: return format_to(ctx.begin(), "Precondition Required");
        case HttpStatus::TooManyRequests: return format_to(ctx.begin(), "Too Many Requests");
        case HttpStatus::RequestHeaderFieldsTooLarge: return format_to(ctx.begin(), "Request Header Fields Too Large");
        case HttpStatus::InternalServerError: return format_to(ctx.begin(), "Internal Server Error");
        case HttpStatus::NotImplemented: return format_to(ctx.begin(), "Not Implemented");
        case HttpStatus::BadGateway: return format_to(ctx.begin(), "Bad Gateway");
        case HttpStatus::ServiceUnavailable: return format_to(ctx.begin(), "Service Unavailable");
        case HttpStatus::GatewayTimeout: return format_to(ctx.begin(), "Gateway Timeout");
        case HttpStatus::HttpVersionNotSupported: return format_to(ctx.begin(), "Http Version Not Supported");
        case HttpStatus::VariantAlsoNegotiates: return format_to(ctx.begin(), "Variant Also Negotiates");
        case HttpStatus::InsufficientStorage: return format_to(ctx.begin(), "Insufficient Storage");
        case HttpStatus::LoopDetected: return format_to(ctx.begin(), "Loop Detected");
        case HttpStatus::BandwidthExceeded: return format_to(ctx.begin(), "Bandwidth Exceeded");
        case HttpStatus::NotExtended: return format_to(ctx.begin(), "Not Extended");
        case HttpStatus::NetworkAuthenticationRequired: return format_to(ctx.begin(), "Network Authentication Required");
        default:
          return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}

