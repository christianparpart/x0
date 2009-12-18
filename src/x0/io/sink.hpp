#ifndef sw_x0_io_sink_hpp
#define sw_x0_io_sink_hpp 1

#include <x0/io/chunk.hpp>

namespace x0 {

class sink
{
public:
	virtual ~sink() {}

	virtual void push(const chunk& data) = 0;

	void operator()(const chunk& data) { push(data); }
};

} // namespace x0

#endif
