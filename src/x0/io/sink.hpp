#ifndef sw_x0_io_sink_hpp
#define sw_x0_io_sink_hpp 1

#include <x0/buffer.hpp>

namespace x0 {

/** chunk consumer.
 *
 * A sink is a chunk consumer, e.g. by saving chunks sequentially
 * into a local file.
 *
 * \see file_sink, buffer_sink, asio_sink, source
 */
class sink
{
public:
	virtual ~sink() {}

	/** pushes given \p data into this sink.
	 *
	 * \param data the data to push into this sink
	 *
	 * \return a buffer view representing the data that could not be written
	 * 		into the sink, e.g., because the write would block, 
	 * 		or an error occurred.
	 * 		If the buffer view is empty, all data could be written
	 * 		successfully.
	 */
	virtual buffer::view push(const buffer::view& data) = 0;
};

} // namespace x0

#endif
