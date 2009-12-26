#ifndef sw_x0_io_filter_source_hpp
#define sw_x0_io_filter_source_hpp 1

#include <x0/io/source.hpp>
#include <x0/io/filter.hpp>

namespace x0 {

class filter_source :
	public source
{
public:
	explicit filter_source(const source& source, const filter& filter) :
		source_(source), filter_(filter) {}

	virtual buffer::view pull(buffer& buf)
	{
		return filter_(source_.pull(buf));
	}
};

} // namespace x0

#endif
