#include <x0/Error.h>
#include <x0/Defines.h>

#include <boost/lexical_cast.hpp>

namespace x0 {

class ErrorCategoryImpl :
	public std::error_category
{
public:
	ErrorCategoryImpl()
	{
	}

	virtual const char *name() const
	{
		return "x0";
	}

	virtual std::string message(int ec) const
	{
		static const char *msg[] = {
			"Success",
			"Config File Error",
			"Fork Error",
			"PID file not specified",
			"Cannot create PID file",
			"Could not initialize SSL library",
			"No HTTP Listeners defined"
		};
		return msg[ec];
	}
};

const std::error_category& errorCategory() throw()
{
	static ErrorCategoryImpl errorCategoryImpl_;
	return errorCategoryImpl_;
}

} // namespace x0

