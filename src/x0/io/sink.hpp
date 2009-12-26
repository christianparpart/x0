#ifndef sw_x0_io_sink_hpp
#define sw_x0_io_sink_hpp 1

#include <x0/buffer.hpp>

namespace x0 {

class sink
{
public:
	virtual ~sink() {}

	virtual buffer::view push(const buffer::view& data) = 0;
};

} // namespace x0

#endif
