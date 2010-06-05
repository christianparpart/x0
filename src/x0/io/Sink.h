#ifndef sw_x0_io_sink_hpp
#define sw_x0_io_sink_hpp 1

#include <x0/Api.h>
#include <memory>

namespace x0 {

class Source;

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
};

typedef std::shared_ptr<Sink> SinkPtr;

//@}

} // namespace x0

#endif
