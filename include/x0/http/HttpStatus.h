#pragma once

/* <x0/HttpStatus.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Api.h>
#include <system_error>

//! \addtogroup http
//@{

namespace x0 {

/**
 * HTTP status code.
 *
 * \see http://www.iana.org/assignments/http-status-codes/http-status-codes.xml
 */
enum class HttpStatus // {{{
{
	Undefined = 0,

	// informational
	ContinueRequest = 100,
	SwitchingProtocols = 101,
	Processing = 102, // WebDAV, RFC 2518

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
	MovedTemporarily = 302,
	Found = MovedTemporarily,
	NotModified = 304,
	TemporaryRedirect = 307, // since HTTP/1.1
	PermanentRedirect = 308, // Internet-Draft

	// client error
	BadRequest = 400,
	Unauthorized = 401,
	PaymentRequired = 402, // reserved for future use
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
	RequestEntityTooLarge = 413,
	RequestUriTooLong = 414,
	UnsupportedMediaType = 415,
	RequestedRangeNotSatisfiable = 416,
	ExpectationFailed = 417,
	ThereAreTooManyConnectionsFromYourIP = 421,
	UnprocessableEntity = 422,
	Locked = 423,
	FailedDependency = 424,
	UnorderedCollection = 425,
	UpgradeRequired = 426,
	PreconditionRequired = 428, // RFC 6585
	TooManyRequests = 429, // RFC 6585
	RequestHeaderFieldsTooLarge = 431, // RFC 6585
	NoResponse = 444, // nginx ("Used in Nginx logs to indicate that the server has returned no information to the client and closed the connection")

	// server error
	InternalServerError = 500,
	NotImplemented = 501,
	BadGateway = 502,
	ServiceUnavailable = 503,
	GatewayTimeout = 504,
	HttpVersionNotSupported = 505,
	VariantAlsoNegotiates = 506, // RFC 2295
	InsufficientStorage = 507, // WebDAV, RFC 4918
	LoopDetected = 508, // WebDAV, RFC 5842
	BandwidthExceeded = 509, // Apache
	NotExtended = 510, // RFC 2774
	NetworkAuthenticationRequired = 511 // RFC 6585
};
// }}}

inline bool operator!(HttpStatus st) { return st == HttpStatus::Undefined; }

X0_API const std::error_category& http_category() throw();

X0_API std::error_code make_error_code(HttpStatus ec);
X0_API std::error_condition make_error_condition(HttpStatus ec);

X0_API bool content_forbidden(HttpStatus code);

} // namespace x0

namespace std {
	// implicit conversion from HttpStatus to error_code
	template<> struct X0_API is_error_code_enum<x0::HttpStatus> : public true_type {};
}

//@}

// {{{ inlines
namespace x0 {

inline std::error_code make_error_code(HttpStatus ec)
{
	return std::error_code(static_cast<int>(ec), http_category());
}

inline std::error_condition make_error_condition(HttpStatus ec)
{
	return std::error_condition(static_cast<int>(ec), http_category());
}

inline bool content_forbidden(HttpStatus code)
{
	switch (code)
	{
		case /*100*/ HttpStatus::ContinueRequest:
		case /*101*/ HttpStatus::SwitchingProtocols:
		case /*204*/ HttpStatus::NoContent:
		case /*205*/ HttpStatus::ResetContent:
		case /*304*/ HttpStatus::NotModified:
			return true;
		default:
			return false;
	}
}

} // namespace x0
// }}}
