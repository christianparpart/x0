#ifndef sw_x0_io_fd_sink_hpp
#define sw_x0_io_fd_sink_hpp 1

#include <x0/io/sink.hpp>

namespace x0 {

class fd_sink :
	public sink
{
public:
	explicit fd_sink(int fd);

	operator bool() const;
	bool operator !() const;

	virtual void push(const chunk& data);

protected:
	int handle_;
};

} // namespace x0

#endif
