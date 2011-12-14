/* <x0/HttpError.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpError.h>
#include <x0/Defines.h>

namespace x0 {

class http_error_category_impl :
	public std::error_category
{
private:
	std::string codes_[600];

	void set(HttpError ec, const std::string& message)
	{
		codes_[static_cast<int>(ec)] = message;
	}

	void initialize_codes()
	{
		for (std::size_t i = 0; i < sizeof(codes_) / sizeof(*codes_); ++i)
			codes_[i] = "Undefined";

		set(HttpError::ContinueRequest, "Continue");
		set(HttpError::SwitchingProtocols, "Switching Protocols");
		set(HttpError::Processing, "Processing");

		set(HttpError::Ok, "Ok");
		set(HttpError::Created, "Created");
		set(HttpError::Accepted, "Accepted");
		set(HttpError::NonAuthoriativeInformation, "Non Authoriative Information");
		set(HttpError::NoContent, "No Content");
		set(HttpError::ResetContent, "Reset Content");
		set(HttpError::PartialContent, "Partial Content");

		set(HttpError::MultipleChoices, "Multiple Choices");
		set(HttpError::MovedPermanently, "Moved Permanently");
		set(HttpError::MovedTemporarily, "Moved Temporarily");
		set(HttpError::NotModified, "Not Modified");

		set(HttpError::BadRequest, "Bad Request");
		set(HttpError::Unauthorized, "Unauthorized");
		set(HttpError::Forbidden, "Forbidden");
		set(HttpError::NotFound, "Not Found");
		set(HttpError::MethodNotAllowed, "Method Not Allowed");
		set(HttpError::NotAcceptable, "Not Acceptable");
		set(HttpError::ProxyAuthenticationRequired, "Proxy Authentication Required");
		set(HttpError::RequestTimeout, "Request Timeout");
		set(HttpError::Conflict, "Conflict");
		set(HttpError::Gone, "Gone");
		set(HttpError::LengthRequired, "Length Required");
		set(HttpError::PreconditionFailed, "Precondition Failed");
		set(HttpError::RequestEntityTooLarge, "Request Entity Too Large");
		set(HttpError::RequestUriTooLong, "Request URI Too Long");
		set(HttpError::UnsupportedMediaType, "Unsupported Media Type");
		set(HttpError::RequestedRangeNotSatisfiable, "Requested Range Not Satisfiable");
		set(HttpError::ExpectationFailed, "Expectation Failed");
		set(HttpError::ThereAreTooManyConnectionsFromYourIP, "There Are Too Many Connections From Your IP");
		set(HttpError::UnprocessableEntity, "Unprocessable Entity");
		set(HttpError::Locked, "Locked");
		set(HttpError::FailedDependency, "Failed Dependency");
		set(HttpError::UnorderedCollection, "Unordered Collection");
		set(HttpError::UpgradeRequired, "Upgrade Required");

		set(HttpError::InternalServerError, "Internal Server Error");
		set(HttpError::NotImplemented, "Not Implemented");
		set(HttpError::BadGateway, "Bad Gateway");
		set(HttpError::ServiceUnavailable, "Service Unavailable");
		set(HttpError::GatewayTimedout, "Gateway Timedout");
		set(HttpError::HttpVersionNotSupported, "HTTP Version Not Supported");
		set(HttpError::InsufficientStorage, "Insufficient Storage");
	}

public:
	http_error_category_impl()
	{
		initialize_codes();
	}

	virtual const char *name() const
	{
		return "http";
	}

	virtual std::string message(int ec) const
	{
		return codes_[ec];
	}
};

http_error_category_impl http_category_impl_;

const std::error_category& http_category() throw()
{
	return http_category_impl_;
}

} // namespace x0
