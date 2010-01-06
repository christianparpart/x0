#ifndef sw_x0_io_file_sink_hpp
#define sw_x0_io_file_sink_hpp 1

#include <x0/fd_sink.hpp>
#include <string>

namespace x0 {

class X0_API file_sink :
	public fd_sink
{
public:
	explicit file_sink(const std::string& filename);
	~file_sink();
};

} // namespace x0

#endif
