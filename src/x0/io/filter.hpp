#ifndef sw_x0_io_filter_hpp
#define sw_x0_io_filter_hpp 1

#include <x0/buffer.hpp>
#include <x0/io/source.hpp>
#include <x0/io/sink.hpp>

namespace x0 {

/** unidirectional data processor.
 *
 * a filter is a processor, that reads from a source, and passes 
 * the received data to the sink. this data may or may not be
 * transformed befor passing to the sink.
 *
 * \see source, sink
 */
class filter
{
public:
	virtual buffer process(const buffer::view& input) = 0;

	buffer operator()(const buffer::view& input)
	{
		return process(input);
	}

#if 0
	//! pumps source through this filter, storing the result into the sink
	void once(source& source, sink& sink)
	{
		buffer b;
		if (buffer::view chunk = source.pull(b))
			sink(process(chunk));
	}

	void all(source& source, sink& sink)
	{
		buffer b;
		while (buffer::view c = source.pull(b))
			sink.push(process(c));
	}
#endif
};

class chained_filter :
	public filter
{
private:
	filter left_;
	filter right_;

public:
	chained_filter(const filter& left, const filter& right) :
		left_(left), right_(right)
	{
	}
#if 0
	virtual buffer process(const buffer& data)
	{
		return right_(left_(data));
	}
#endif
};

chained_filter chain(const source&, const filter&);
chained_filter chain(const filter&, const filter&);
chained_filter chain(const filter&, const sink&);

} // namespace x0

#endif
