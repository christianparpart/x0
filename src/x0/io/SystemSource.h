#ifndef sw_x0_io_SystemSource_hpp
#define sw_x0_io_SystemSource_hpp 1

#include <x0/io/Source.h>

namespace x0 {

//! \addtogroup io
//@{

/** file descriptor stream source.
 */
class X0_API SystemSource :
	public Source
{
public:
	explicit SystemSource(int fd);
	SystemSource(int fd, std::size_t offset, std::size_t count);

	operator bool() const;
	bool operator !() const;

	std::size_t offset() const;
	std::size_t count() const;

	void async(bool value);
	bool async() const;

	int handle() const;

	virtual BufferRef pull(Buffer& buf);
	virtual bool eof() const;

	virtual void accept(SourceVisitor& v);

protected:
	int handle_;
	std::size_t offset_;
	std::size_t count_;
};

// {{{ inlines
/** initializes SystemSource with a non-seekable file descriptor.
 */
inline SystemSource::SystemSource(int fd) :
	handle_(fd),
	offset_(static_cast<std::size_t>(-1)),
	count_(static_cast<std::size_t>(-1))
{
}

/** initializes SystemSource witha seekable file descriptor.
 */
inline SystemSource::SystemSource(int fd, std::size_t offset, std::size_t count) :
	handle_(fd),
	offset_(offset),
	count_(count)
{
}

inline int SystemSource::handle() const
{
	return handle_;
}

inline std::size_t SystemSource::offset() const
{
	return offset_;
}

inline std::size_t SystemSource::count() const
{
	return count_;
}
// }}}

//@}

} // namespace x0

#endif
