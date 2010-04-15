#include <x0/gai_error.hpp>
#include <x0/defines.hpp>

#include <netdb.h>

namespace x0 {

class gai_error_category_impl :
	public std::error_category
{
public:
	gai_error_category_impl()
	{
	}

	virtual const char *name() const
	{
		return "gai";
	}

	virtual std::string message(int ec) const
	{
		return gai_strerror(ec);
	}
};

gai_error_category_impl gai_category_impl_;

const std::error_category& gai_category() throw()
{
	return gai_category_impl_;
}

} // namespace x0
