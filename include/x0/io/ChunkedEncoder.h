/* <ChunkedEncoder.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_ChunkedEncoder_hpp
#define sw_x0_io_ChunkedEncoder_hpp 1

#include <x0/io/Filter.h>

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

    virtual Buffer process(const BufferRef& data);

private:
    bool finished_;
};

//@}

} // namespace x0

#endif
