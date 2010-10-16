/* <BufferSink.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_buffer_sink_hpp
#define sw_x0_io_buffer_sink_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Sink.h>
#include <x0/io/Source.h>

namespace x0 {

//! \addtogroup io
//@{

/** sink, storing incoming data into a buffer.
 *
 * \see sink, source
 */
class X0_API BufferSink :
	public Sink
{
public:
	BufferSink() :
		buffer_()
	{
	}

	virtual ssize_t pump(Source& src)
	{
		return src.pull(buffer_).size();
	}

public:
	void clear()
	{
		buffer_.clear();
	}

	const Buffer& buffer() const
	{
		return buffer_;
	}

	std::size_t size() const
	{
		return buffer_.size();
	}

	bool empty() const
	{
		return buffer_.empty();
	}

private:
	Buffer buffer_;
};

//@}

} // namespace x0

#endif
