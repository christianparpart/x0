#ifndef sw_x0_io_sink_hpp
#define sw_x0_io_sink_hpp 1

#include <x0/api.hpp>
#include <memory>

namespace x0 {

class source;

//! \addtogroup io
//@{

enum class pump_state
{
	complete,
	partial,
	error
};

/** chunk consumer.
 *
 * A sink is a chunk consumer, e.g. by saving chunks sequentially
 * into a local file.
 *
 * \see file_sink, buffer_sink, asio_sink, source
 */
class X0_API sink
{
public:
	virtual ~sink() {}

	/** pumps given source into this sink.
	 *
	 * \param src The source to pump intto this sink.
	 *
	 * \return number of bytes pumped through
	 */
	virtual pump_state pump(source& src) = 0;
};

typedef std::shared_ptr<sink> sink_ptr;

//@}

} // namespace x0

#endif
