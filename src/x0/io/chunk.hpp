#ifndef sw_x0_io_chunk_hpp
#define sw_x0_io_chunk_hpp 1

#include <stdint.h>
#include <stddef.h>

namespace x0 {

class chunk;
class memory_chunk;
class fd_chunk;

class chunk_visitor
{
public:
	~chunk_visitor() {}

	virtual void visit(const memory_chunk&);
	virtual void visit(const fd_chunk&);
};

/** a chunk of data being read, or to be written
 *
 * \note chunks may be created on the fly, but are read-only in access after construction.
 * \see memory_chunk, fd_chunk
 */
class chunk
{
public:
	virtual void accept(chunk_visitor& visitor) const /*= 0*/;
	operator bool() const;
};

class memory_chunk :
	public chunk
{
private:
	const char *buffer_;
	size_t size_;

public:
	typedef const char *iterator;

public:
	memory_chunk(const char *buffer, size_t size) :
		buffer_(buffer),
		size_(size)
	{
	}

	template<typename PodType, size_t N>
	memory_chunk(PodType (&value)[N]) :
		buffer_(value),
		size_(N - 1)
	{
	}

	// STL-like read-only access
	const char *begin() const { return buffer_; }
	const char *end() const { return buffer_ + size_; }
	size_t size() const { return size_; }
};

class fd_chunk :
	public chunk
{
private:
	int handle_;
	size_t offset_;
	size_t size_;

public:
	int handle() const;
	size_t offset() const;
	size_t size() const;
};

} // namespace x0

#endif
