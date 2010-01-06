#ifndef sw_x0_io_fd_sink_hpp
#define sw_x0_io_fd_sink_hpp 1

#include <x0/sink.hpp>

namespace x0 {

class X0_API fd_sink :
	public sink
{
public:
	explicit fd_sink(int fd);

	operator bool() const;
	bool operator !() const;

	void async(bool value);
	bool async() const;

	virtual buffer::view push(const buffer::view& data);

protected:
	int handle_;
};

} // namespace x0

#endif
