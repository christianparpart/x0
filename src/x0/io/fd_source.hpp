#ifndef sw_x0_io_fd_source_hpp
#define sw_x0_io_fd_source_hpp 1

#include <x0/io/source.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** file descriptor stream source.
 */
class X0_API fd_source :
	public source
{
public:
	explicit fd_source(int fd);
	fd_source(int fd, std::size_t offset, std::size_t count);

	operator bool() const;
	bool operator !() const;

	std::size_t offset() const;
	std::size_t count() const;

	void async(bool value);
	bool async() const;

	int handle() const;

	virtual buffer_ref pull(buffer& buf);

	virtual void accept(source_visitor& v);

protected:
	int handle_;
	std::size_t offset_;
	std::size_t count_;
};

// {{{ inlines
/** initializes fd_source with a non-seekable file descriptor.
 */
inline fd_source::fd_source(int fd) :
	handle_(fd),
	offset_(static_cast<std::size_t>(-1)),
	count_(static_cast<std::size_t>(-1))
{
}

/** initializes fd_source witha seekable file descriptor.
 */
inline fd_source::fd_source(int fd, std::size_t offset, std::size_t count) :
	handle_(fd),
	offset_(offset),
	count_(count)
{
}

inline int fd_source::handle() const
{
	return handle_;
}

inline std::size_t fd_source::offset() const
{
	return offset_;
}

inline std::size_t fd_source::count() const
{
	return count_;
}
// }}}

//@}

} // namespace x0

#endif
