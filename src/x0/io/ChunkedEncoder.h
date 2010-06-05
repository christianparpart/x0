#ifndef sw_x0_io_ChunkedEncoder_hpp
#define sw_x0_io_ChunkedEncoder_hpp 1

#include <x0/io/Filter.h>
#include <zlib.h>

namespace x0 {

//! \addtogroup io
//@{

/**
 */
class X0_API ChunkedEncoder :
	public Filter
{
public:
	ChunkedEncoder();

	virtual Buffer process(const BufferRef& data, bool eof);

private:
	bool finished_;
};

//@}

} // namespace x0

#endif
