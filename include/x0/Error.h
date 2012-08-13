/* <Error.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_Error_h
#define sw_x0_Error_h (1)

#include <x0/Api.h>
#include <system_error>

namespace x0 {

enum class Error // {{{
{
	Success = 0,
	ConfigFileError,
	ForkError,
	PidFileNotSpecified,
	CannotCreatePidFile,
	CouldNotInitializeSslLibrary,
	NoListenersDefined
}; // }}}

const std::error_category& errorCategory() throw();

std::error_code make_error_code(Error ec);
std::error_condition make_error_condition(Error ec);

} // namespace x0

namespace std {
	// implicit conversion from x0::Error to error_code
	template<> struct is_error_code_enum<x0::Error> : public true_type {};
}

// {{{ inlines
namespace x0 {

inline std::error_code make_error_code(Error ec)
{
	return std::error_code(static_cast<int>(ec), errorCategory());
}

inline std::error_condition make_error_condition(Error ec)
{
	return std::error_condition(static_cast<int>(ec), errorCategory());
}

} // namespace x0
// }}}

#endif
