/* <Sink.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_sink_hpp
#define sw_x0_io_sink_hpp 1

#include <x0/Api.h>
#include <memory>

namespace x0 {

class Source;
class SinkVisitor;

//! \addtogroup io
//@{

/** chunk consumer.
 *
 * A sink is a chunk consumer, e.g. by saving chunks sequentially
 * into a local file.
 *
 * \see file_sink, buffer_sink, asio_sink, source
 */
class X0_API Sink
{
public:
	virtual ~Sink() {}

	virtual void accept(SinkVisitor& v) = 0;

	virtual ssize_t write(const void *buffer, size_t size) = 0;
};

//@}

} // namespace x0

#endif
