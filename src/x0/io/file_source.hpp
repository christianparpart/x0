#ifndef sw_x0_io_file_source_hpp
#define sw_x0_io_file_source_hpp 1

#include <x0/io/fd_source.hpp>
#include <string>

namespace x0 {

class file_source :
	public fd_source
{
public:
	explicit file_source(const std::string& filename);
	~file_source();
};

} // namespace x0

#endif
