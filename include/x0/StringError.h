/* <StringError.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_StringError_h
#define sw_x0_StringError_h

#include <x0/Api.h>
#include <vector>
#include <system_error>

namespace x0 {

enum class StringError
{
	Success = 0,
	GenericError = 1
};

class X0_API StringErrorCategoryImpl :
	public std::error_category
{
private:
	std::vector<std::string> vector_;

public:
	StringErrorCategoryImpl();
	~StringErrorCategoryImpl() noexcept(true);

	int get(const std::string& msg);

	virtual const char *name() const noexcept(true);
	virtual std::string message(int ec) const;
};

StringErrorCategoryImpl& stringErrorCategory() throw();

std::error_code make_error_code(const std::string& msg);
std::error_condition make_error_condition(const std::string& msg);

} // namespace x0

namespace std {
	// implicit conversion from gai_error to error_code
	template<> struct is_error_code_enum<x0::StringError> : public true_type {};
}

// {{{ inlines
namespace x0 {

inline std::error_code make_error_code(const std::string& msg)
{
	return std::error_code(stringErrorCategory().get(msg), stringErrorCategory());
}

inline std::error_condition make_error_condition(const std::string& msg)
{
	return std::error_condition(stringErrorCategory().get(msg), stringErrorCategory());
}

}
// }}}

#endif
