#ifndef sw_x0_io_filter_hpp
#define sw_x0_io_filter_hpp 1

#include <x0/io/chunk.hpp>
#include <x0/io/source.hpp>
#include <x0/io/sink.hpp>

namespace x0 {

class filter
{
public:
	virtual chunk process(const chunk& input);

	chunk operator()(const chunk& input)
	{
		return process(input);
	}

	//! pumps source through this filter, storing the result into the sink
	void once(source& source, sink& sink)
	{
		if (chunk c = source())
			sink(process(c));
	}

	void all(source& source, sink& sink)
	{
		while (chunk c = source())
			sink.push(process(c));
	}
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

	virtual chunk process(const chunk& data)
	{
		return right_(left_(data));
	}
};

chained_filter chain(const source&, const filter&);
chained_filter chain(const filter&, const filter&);
chained_filter chain(const filter&, const sink&);

} // namespace x0

#endif
