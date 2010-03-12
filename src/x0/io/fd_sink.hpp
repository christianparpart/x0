#ifndef sw_x0_io_fd_sink_hpp
#define sw_x0_io_fd_sink_hpp 1

#include <x0/io/sink.hpp>
#include <x0/io/buffer.hpp>

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

	void async(bool value);
	bool async() const;

	int handle() const;

	virtual ssize_t pump(source& src);

protected:
	buffer buf_;
	off_t offset_;
	int handle_;
};

//@}

inline int fd_sink::handle() const
{
	return handle_;
}

} // namespace x0

#endif
