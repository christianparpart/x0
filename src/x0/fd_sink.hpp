#ifndef sw_x0_io_fd_sink_hpp
#define sw_x0_io_fd_sink_hpp 1

#include <x0/sink.hpp>
#include <x0/buffer.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** file descriptor stream sink.
 */
class X0_API fd_sink :
	public sink
{
public:
	explicit fd_sink(int fd);

	operator bool() const;
	bool operator !() const;

	void async(bool value);
	bool async() const;

	virtual std::size_t pump(source& src);

protected:
	buffer buf_;
	int handle_;
};

//@}

} // namespace x0

#endif
