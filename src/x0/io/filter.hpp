#ifndef sw_x0_io_filter_hpp
#define sw_x0_io_filter_hpp 1

#include <x0/buffer.hpp>
#include <x0/io/source.hpp>
#include <x0/io/sink.hpp>
#include <x0/api.hpp>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** unidirectional data processor.
 *
 * a filter is a processor, that reads from a source, and passes 
 * the received data to the sink. this data may or may not be
 * transformed befor passing to the sink.
 *
 * \see source, sink
 */
class X0_API filter
{
public:
	/** processes given input data through this filter. */
	virtual buffer process(const buffer_ref& input) = 0;

public:
	buffer operator()(const buffer_ref& input)
	{
		return process(input);
	}
};

typedef std::shared_ptr<filter> filter_ptr;

//@}

} // namespace x0

#endif
