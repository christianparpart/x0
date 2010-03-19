#ifndef sw_x0_io_source_hpp
#define sw_x0_io_source_hpp 1

#include <x0/buffer.hpp>
#include <x0/io/sink.hpp>
#include <x0/api.hpp>
#include <memory>

namespace x0 {

class source_visitor;

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
	virtual buffer_ref pull(buffer& buf) = 0;

	/** every derivate has to implement this to fullfill the visitor-pattern.
	 *
	 * <code>v.visit(*this); // ideal example implementation inside derived class.</code>
	 */
	virtual void accept(source_visitor& v) = 0;

	/** pumps this source into given sink, \p output.
	 *
	 * \param output sink to pump the data from this source to.
	 * \return number of bytes pumped into the sink.
	 */
	pump_state pump(sink& output);
};

typedef std::shared_ptr<source> source_ptr;

// {{{ inlines
inline pump_state source::pump(sink& output)
{
	return output.pump(*this);
}
// }}}

//@}

} // namespace x0

#endif
