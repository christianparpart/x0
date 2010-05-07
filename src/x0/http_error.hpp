#ifndef sw_x0_http_error_hpp
#define sw_x0_http_error_hpp (1)

#include <system_error>

enum class http_error // {{{
{
	continue_ = 100,
	switching_protocols = 101,
	processing = 102,

	ok = 200,
	created = 201,
	accepted = 202,
	non_authoriative_information = 203,
	no_content = 204,
	reset_content = 205,
	partial_content = 206,

	multiple_choices = 300,
	moved_permanently = 301,
	moved_temporarily = 302,
	not_modified = 304,

	bad_request = 400,
	unauthorized = 401,
	forbidden = 403,
	not_found = 404,
	method_not_allowed = 405,
	not_acceptable = 406,
	proxy_authentication_required = 407,
	request_timeout = 408,
	conflict = 409,
	gone = 410,
	length_required = 411,
	precondition_failed = 412,
	request_entity_too_large = 413,
	request_uri_too_long = 414,
	unsupported_media_type = 415,
	requested_range_not_satisfiable = 416,
	expectation_failed = 417,
	there_are_too_many_connections_from_your_ip = 421,
	unprocessable_entity = 422,
	locked = 423,
	failed_dependency = 424,
	unordered_collection = 425,
	upgrade_required = 426,

	internal_server_error = 500,
	not_implemented = 501,
	bad_gateway = 502,
	service_unavailable = 503,
	gateway_timedout = 504,
	http_version_not_supported = 505,
	insufficient_storage = 507
};
// }}}

const std::error_category& http_category() throw();

std::error_code make_error_code(http_error ec);
std::error_condition make_error_condition(http_error ec);

bool content_forbidden(http_error code);

namespace std {
	// implicit conversion from http_error to error_code
	template<> struct is_error_code_enum<http_error> : public true_type {};
}

// {{{ inlines
inline std::error_code make_error_code(http_error ec)
{
	return std::error_code(static_cast<int>(ec), http_category());
}

inline std::error_condition make_error_condition(http_error ec)
{
	return std::error_condition(static_cast<int>(ec), http_category());
}

inline bool content_forbidden(http_error code)
{
	switch (code)
	{
		case /*100*/ http_error::continue_:
		case /*101*/ http_error::switching_protocols:
		case /*204*/ http_error::no_content:
		case /*205*/ http_error::reset_content:
		case /*304*/ http_error::not_modified:
			return true;
		default:
			return false;
	}
}

// }}}

#endif
