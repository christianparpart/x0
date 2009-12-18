#ifndef sw_x0_io_fd_source_hpp
#define sw_x0_io_fd_source_hpp 1

#include <x0/io/source.hpp>

namespace x0 {

class fd_source :
	public source
{
public:
	explicit fd_source(int fd);

	operator bool() const;
	bool operator !() const;

	virtual chunk pull();

protected:
	int handle_;
};

} // namespace x0

#endif
