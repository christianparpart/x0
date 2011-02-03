/* <Sink.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
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

	/** pumps given source into this sink.
	 *
	 * \param src The source to pump intto this sink.
	 *
	 * \return number of bytes pumped through
	 */
	virtual ssize_t pump(Source& src) = 0;

	virtual void accept(SinkVisitor& v) = 0;

	virtual ssize_t write(const void *buffer, size_t size) = 0;
};

//@}

} // namespace x0

#endif
