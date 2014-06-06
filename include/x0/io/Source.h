/* <Source.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_source_hpp
#define sw_x0_io_source_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Sink.h>
#include <x0/Api.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** chunk producer.
 *
 * A source is a chunk producer, e.g. by reading sequentially from a file.
 *
 * \see FileSource, Sink, Filter
 */
class X0_API Source
{
public:
    virtual ~Source() {}

    virtual ssize_t sendto(Sink& output) = 0;
    virtual ssize_t size() const = 0;

    virtual const char* className() const = 0;
};

//@}

} // namespace x0

#endif
