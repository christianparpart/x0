#ifndef sw_x0_io_chunked_filter_hpp
#define sw_x0_io_chunked_filter_hpp 1

#include <x0/io/filter.hpp>
#include <zlib.h>

namespace x0 {

//! \addtogroup io
//@{

/**
 */
class X0_API chunked_filter :
	public filter
{
public:
	chunked_filter();

	virtual buffer process(const buffer_ref& data, bool eof);

private:
	bool finished_;
};

//@}

} // namespace x0

#endif
