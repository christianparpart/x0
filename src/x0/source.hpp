#ifndef sw_x0_io_source_hpp
#define sw_x0_io_source_hpp 1

#include <x0/buffer.hpp>
#include <x0/api.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** chunk producer.
 *
 * A source is a chunk producer, e.g. by reading sequentially from a file.
 * \see file_source, sink, filter
 */
class X0_API source
{
public:
	virtual ~source() {}

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
	virtual buffer::view pull(buffer&) = 0;
};

//@}

} // namespace x0

#endif
