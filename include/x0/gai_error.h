/* <x0/gai_error.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_gai_error_hpp
#define sw_x0_gai_error_hpp (1)

#include <system_error>

namespace x0 {

enum class gai_error // {{{
{
	success = 0,
	unknown = -1
};
// }}}

const std::error_category& gai_category() throw();

std::error_code make_error_code(gai_error ec);
std::error_condition make_error_condition(gai_error ec);

} // namespace x0

namespace std {
	// implicit conversion from gai_error to error_code
	template<> struct is_error_code_enum<x0::gai_error> : public true_type {};
}

// {{{ inlines
namespace x0 {

inline std::error_code make_error_code(gai_error ec)
{
	return std::error_code(static_cast<int>(ec), gai_category());
}

inline std::error_condition make_error_condition(gai_error ec)
{
	return std::error_condition(static_cast<int>(ec), gai_category());
}

} // namespace x0
// }}}

#endif
