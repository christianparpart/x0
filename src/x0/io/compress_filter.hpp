#ifndef sw_x0_io_compress_filter_hpp
#define sw_x0_io_compress_filter_hpp 1

#include <x0/io/filter.hpp>
#include <zlib.h>

namespace x0 {

//! \addtogroup io
//@{

/** simply transforms all letters into upper-case letters (test filter).
 */
class X0_API compress_filter :
	public filter
{
public:
	compress_filter();

	virtual buffer process(const buffer_ref& data);

private:
	z_stream z_;
};

//@}

} // namespace x0

#endif
