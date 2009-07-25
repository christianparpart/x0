#ifndef composite_buffer_writer_hpp
#define composite_buffer_writer_hpp 1

#include <x0/composite_buffer.hpp>

namespace x0 {

/**
 *
 * function object class for writing a composite_buffer to a target.
 *
 * The template `Target` must support: `unspecified write(void *buffer, std::size_t size)`.
 */
template<class Target>
class composite_buffer_writer :
	public composite_buffer::visitor
{
private:
	Target& target_;
	const composite_buffer& cb_;
	std::size_t nwritten_;

public:
	composite_buffer_writer(Target& t, const composite_buffer& cb);

	std::size_t operator()();
	std::size_t write();

private:
	virtual void visit(const composite_buffer::iovec_chunk&);
	virtual void visit(const composite_buffer::fd_chunk&);
};

template<class Target>
std::size_t write(Target& t, const composite_buffer& cb);

// {{{ impl
template<class Target>
inline composite_buffer_writer<Target>::composite_buffer_writer(Target& target, const composite_buffer& cb) :
	target_(target),
	cb_(cb),
	nwritten_(0)
{
}

template<class Target>
inline std::size_t composite_buffer_writer<Target>::operator()()
{
	return write();
}

template<class Target>
std::size_t composite_buffer_writer<Target>::write()
{
	for (auto i = cb_.begin(), e = cb_.end(); i != e; ++i)
	{
		i->accept(*this);
	}

	return nwritten_;
}

template<class Target>
inline void composite_buffer_writer<Target>::visit(const composite_buffer::iovec_chunk& chunk)
{
	for (int i = 0, e = chunk.length(); i != e; ++i)
	{
		target_.write(static_cast<char *>(chunk[i].iov_base), chunk[i].iov_len);
		nwritten_ += chunk[i].iov_len;
	}
}

template<class Target>
inline void composite_buffer_writer<Target>::visit(const composite_buffer::fd_chunk& chunk)
{
	if (scoped_mmap map = scoped_mmap(NULL, chunk.size(), PROT_READ, MAP_PRIVATE, chunk.fd(), 0))
	{
		target_.write(map.address<char>() + chunk.offset(), chunk.size());
		nwritten_ += chunk.size();
	}
}

template<class Target>
std::size_t write(Target& t, const composite_buffer& cb)
{
	return composite_buffer_writer<Target>(t, cb)();
}
// }}}

} // namespace x0

#endif
