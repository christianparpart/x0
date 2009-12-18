#ifndef sw_x0_io_null_filter_hpp
#define sw_x0_io_null_filter_hpp 1

#include <x0/io/filter.hpp>

namespace x0 {

/** simply passes incoming chunks through */
class null_filter :
	public filter
{
public:
	null_filter() {}

	virtual chunk process(const chunk& data)
	{
		return data;
	}
};

} // namespace x0

#endif
