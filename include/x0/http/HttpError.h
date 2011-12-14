/* <x0/HttpError.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_http_error_hpp
#define sw_x0_http_error_hpp (1)

#include <x0/Api.h>
#include <system_error>

//! \addtogroup http
//@{

namespace x0 {

enum class HttpError // {{{
{
	Undefined = 0,

	// informational
	ContinueRequest = 100,
	SwitchingProtocols = 101,
	Processing = 102,

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

	// client error
	BadRequest = 400,
	Unauthorized = 401,
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

	// server error
	InternalServerError = 500,
	NotImplemented = 501,
	BadGateway = 502,
	ServiceUnavailable = 503,
	GatewayTimedout = 504,
	HttpVersionNotSupported = 505,
	InsufficientStorage = 507
};
// }}}

X0_API const std::error_category& http_category() throw();

X0_API std::error_code make_error_code(HttpError ec);
X0_API std::error_condition make_error_condition(HttpError ec);

X0_API bool content_forbidden(HttpError code);

} // namespace x0

namespace std {
	// implicit conversion from HttpError to error_code
	template<> struct X0_API is_error_code_enum<x0::HttpError> : public true_type {};
}

//@}

// {{{ inlines
namespace x0 {

inline std::error_code make_error_code(HttpError ec)
{
	return std::error_code(static_cast<int>(ec), http_category());
}

inline std::error_condition make_error_condition(HttpError ec)
{
	return std::error_condition(static_cast<int>(ec), http_category());
}

inline bool content_forbidden(HttpError code)
{
	switch (code)
	{
		case /*100*/ HttpError::ContinueRequest:
		case /*101*/ HttpError::SwitchingProtocols:
		case /*204*/ HttpError::NoContent:
		case /*205*/ HttpError::ResetContent:
		case /*304*/ HttpError::NotModified:
			return true;
		default:
			return false;
	}
}

} // namespace x0
// }}}

#endif
