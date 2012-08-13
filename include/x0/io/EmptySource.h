/* <EmptySource.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_EmptySource_hpp
#define sw_x0_io_EmptySource_hpp 1

#include <x0/io/Source.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** an empty source, sending nothing.
 *
 * \see Buffer, Source, Sink
 */
class X0_API EmptySource :
	public Source
{
public:
	virtual ssize_t sendto(Sink& sink);

	virtual const char* className() const;
};

//@}

} // namespace x0

#endif
