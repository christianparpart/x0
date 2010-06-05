#include <x0/http/HttpError.h>
#include <x0/Defines.h>

namespace x0 {

class http_error_category_impl :
	public std::error_category
{
private:
	std::string codes_[600];

	void set(http_error ec, const std::string& message)
	{
		codes_[static_cast<int>(ec)] = message;
	}

	void initialize_codes()
	{
		for (std::size_t i = 0; i < sizeof(codes_) / sizeof(*codes_); ++i)
			codes_[i] = "Undefined";

		set(http_error::continue_request, "Continue");
		set(http_error::switching_protocols, "Switching Protocols");
		set(http_error::processing, "Processing");

		set(http_error::ok, "Ok");
		set(http_error::created, "Created");
		set(http_error::accepted, "Accepted");
		set(http_error::non_authoriative_information, "Non Authoriative Information");
		set(http_error::no_content, "No Content");
		set(http_error::reset_content, "Reset Content");
		set(http_error::partial_content, "Partial Content");

		set(http_error::multiple_choices, "Multiple Choices");
		set(http_error::moved_permanently, "Moved Permanently");
		set(http_error::moved_temporarily, "Moved Temporarily");
		set(http_error::not_modified, "Not Modified");

		set(http_error::bad_request, "Bad Request");
		set(http_error::unauthorized, "Unauthorized");
		set(http_error::forbidden, "Forbidden");
		set(http_error::not_found, "Not Found");
		set(http_error::method_not_allowed, "Method Not Allowed");
		set(http_error::not_acceptable, "Not Acceptable");
		set(http_error::proxy_authentication_required, "Proxy Authentication Required");
		set(http_error::request_timeout, "Request Timeout");
		set(http_error::conflict, "Conflict");
		set(http_error::gone, "Gone");
		set(http_error::length_required, "Length Required");
		set(http_error::precondition_failed, "Precondition Failed");
		set(http_error::request_entity_too_large, "Request Entity Too Large");
		set(http_error::request_uri_too_long, "Request URI Too Long");
		set(http_error::unsupported_media_type, "Unsupported Media Type");
		set(http_error::requested_range_not_satisfiable, "Requested Range Not Satisfiable");
		set(http_error::expectation_failed, "Expectation Failed");
		set(http_error::there_are_too_many_connections_from_your_ip, "There Are Too Many Connections From Your IP");
		set(http_error::unprocessable_entity, "Unprocessable Entity");
		set(http_error::locked, "Locked");
		set(http_error::failed_dependency, "Failed Dependency");
		set(http_error::unordered_collection, "Unordered Collection");
		set(http_error::upgrade_required, "Upgrade Required");

		set(http_error::internal_server_error, "Internal Server Error");
		set(http_error::not_implemented, "Not Implemented");
		set(http_error::bad_gateway, "Bad Gateway");
		set(http_error::service_unavailable, "Service Unavailable");
		set(http_error::gateway_timedout, "Gateway Timedout");
		set(http_error::http_version_not_supported, "HTTP Version Not Supported");
		set(http_error::insufficient_storage, "Insufficient Storage");
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
