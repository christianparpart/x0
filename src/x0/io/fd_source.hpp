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

	void async(bool value);
	bool async() const;

	virtual buffer::view pull(buffer& buf);

protected:
	int handle_;
};

} // namespace x0

#endif
