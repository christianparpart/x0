/* <NullFilter.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_NullFilter_hpp
#define sw_x0_io_NullFilter_hpp 1

#include <x0/io/Filter.h>
#include <x0/Buffer.h>

namespace x0 {

//! \addtogroup io
//@{

/** simply passes incoming buffers through.
 */
class X0_API NullFilter :
	public Filter
{
public:
	NullFilter() {}

	virtual Buffer process(const BufferRef& data, bool /*eof*/)
	{
		return Buffer(data);
	}
};

//@}

} // namespace x0

#endif
