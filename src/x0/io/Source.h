/* <Source.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_source_hpp
#define sw_x0_io_source_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Sink.h>
#include <x0/Api.h>
#include <memory>

namespace x0 {

class SourceVisitor;

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

	/** pulls new ata from this source.
	 *
	 * Repeative calls to this function should sequentially read 
	 * from this source.
	 *
	 * Although, the passed buffer should never be cleared by this method itself.
	 *
	 * Instead, it may be used as some cheap storage as it might have pre-allocated
	 * some memory already and might contain some important data from 
	 * previous calls to this source's pull() already.
	 *
	 * \param buf the buffer to store the read data in.
	 * \return a view into the buffer to the actually produced data.
	 *
	 * \see sink::push()
	 */
	virtual BufferRef pull(Buffer& buf) = 0;

	/** tests whether this source stream reached EOF (end of file/stream) already.
	 *
	 * \retval true the end of stream is reached, thus, further calls to pull()
	 * 		<b>will</b> result into a no-op.
	 * \retval false there is still more data to read, thus, further calls to pull()
	 * 		<b>will likely</b> provide more data.
	 */
	virtual bool eof() const = 0;

	/** every derivate has to implement this to fullfill the visitor-pattern.
	 *
	 * <code>v.visit(*this); // ideal example implementation inside derived class.</code>
	 */
	virtual void accept(SourceVisitor& v) = 0;

	/** pumps this source into given sink, \p output.
	 *
	 * \param output sink to pump the data from this source to.
	 * \return number of bytes pumped into the sink.
	 */
	ssize_t pump(Sink& output);
};

typedef std::shared_ptr<Source> SourcePtr;

// {{{ inlines
inline ssize_t Source::pump(Sink& output)
{
	return output.pump(*this);
}
// }}}

//@}

} // namespace x0

#endif
