#ifndef sw_x0_io_source_hpp
#define sw_x0_io_source_hpp 1

#include <x0/io/chunk.hpp>

namespace x0 {

class source
{
public:
	virtual ~source() {}

	virtual chunk pull() = 0;

	chunk operator()() { return pull(); }
};

} // namespace x0

#endif
