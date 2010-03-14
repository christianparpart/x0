#ifndef sw_x0_io_null_filter_hpp
#define sw_x0_io_null_filter_hpp 1

#include <x0/io/filter.hpp>
#include <x0/buffer.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** simply passes incoming buffers through.
 */
class X0_API null_filter :
	public filter
{
public:
	null_filter() {}

	virtual buffer process(const buffer_ref& data)
	{
		return buffer(data);
	}
};

//@}

} // namespace x0

#endif
