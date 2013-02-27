/* <x0/HttpStatus.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/http/HttpStatus.h>
#include <x0/Defines.h>

namespace x0 {

class http_error_category_impl :
	public std::error_category
{
private:
	std::string codes_[600];

	void set(HttpStatus ec, const std::string& message)
	{
		codes_[static_cast<int>(ec)] = message;
	}

	void initialize_codes()
	{
		for (std::size_t i = 0; i < sizeof(codes_) / sizeof(*codes_); ++i)
			codes_[i] = "Undefined";

		set(HttpStatus::ContinueRequest, "Continue");
		set(HttpStatus::SwitchingProtocols, "Switching Protocols");
		set(HttpStatus::Processing, "Processing");

		set(HttpStatus::Ok, "Ok");
		set(HttpStatus::Created, "Created");
		set(HttpStatus::Accepted, "Accepted");
		set(HttpStatus::NonAuthoriativeInformation, "Non Authoriative Information");
		set(HttpStatus::NoContent, "No Content");
		set(HttpStatus::ResetContent, "Reset Content");
		set(HttpStatus::PartialContent, "Partial Content");

		set(HttpStatus::MultipleChoices, "Multiple Choices");
		set(HttpStatus::MovedPermanently, "Moved Permanently");
		set(HttpStatus::MovedTemporarily, "Moved Temporarily");
		set(HttpStatus::NotModified, "Not Modified");

		set(HttpStatus::BadRequest, "Bad Request");
		set(HttpStatus::Unauthorized, "Unauthorized");
		set(HttpStatus::Forbidden, "Forbidden");
		set(HttpStatus::NotFound, "Not Found");
		set(HttpStatus::MethodNotAllowed, "Method Not Allowed");
		set(HttpStatus::NotAcceptable, "Not Acceptable");
		set(HttpStatus::ProxyAuthenticationRequired, "Proxy Authentication Required");
		set(HttpStatus::RequestTimeout, "Request Timeout");
		set(HttpStatus::Conflict, "Conflict");
		set(HttpStatus::Gone, "Gone");
		set(HttpStatus::LengthRequired, "Length Required");
		set(HttpStatus::PreconditionFailed, "Precondition Failed");
		set(HttpStatus::RequestEntityTooLarge, "Request Entity Too Large");
		set(HttpStatus::RequestUriTooLong, "Request URI Too Long");
		set(HttpStatus::UnsupportedMediaType, "Unsupported Media Type");
		set(HttpStatus::RequestedRangeNotSatisfiable, "Requested Range Not Satisfiable");
		set(HttpStatus::ExpectationFailed, "Expectation Failed");
		set(HttpStatus::ThereAreTooManyConnectionsFromYourIP, "There Are Too Many Connections From Your IP");
		set(HttpStatus::UnprocessableEntity, "Unprocessable Entity");
		set(HttpStatus::Locked, "Locked");
		set(HttpStatus::FailedDependency, "Failed Dependency");
		set(HttpStatus::UnorderedCollection, "Unordered Collection");
		set(HttpStatus::UpgradeRequired, "Upgrade Required");

		set(HttpStatus::InternalServerError, "Internal Server Error");
		set(HttpStatus::NotImplemented, "Not Implemented");
		set(HttpStatus::BadGateway, "Bad Gateway");
		set(HttpStatus::ServiceUnavailable, "Service Unavailable");
		set(HttpStatus::GatewayTimedout, "Gateway Timedout");
		set(HttpStatus::HttpVersionNotSupported, "HTTP Version Not Supported");
		set(HttpStatus::InsufficientStorage, "Insufficient Storage");
		set(HttpStatus::VariantAlsoNegotiates, "Variant Also Negotiates");
		set(HttpStatus::LoopDetected, "Loop Detected");
		set(HttpStatus::BandwidthExceeded, "Bandwidth Exceeded");
		set(HttpStatus::NotExtended, "Not Extended");
		set(HttpStatus::NetworkAuthenticationRequired, "Network Authentication Required");
	}

public:
	http_error_category_impl()
	{
		initialize_codes();
	}

	virtual const char *name() const noexcept(true)
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
