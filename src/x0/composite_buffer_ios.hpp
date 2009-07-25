#ifndef composite_buffer_ios_hpp
#define composite_buffer_ios_hpp 1

#include <x0/composite_buffer.hpp>
#include <x0/detail/scoped_mmap.hpp>
#include <iosfwd>

std::ostream& operator<<(std::ostream& os, const x0::composite_buffer& cb);
std::ostream& operator<<(std::ostream& os, const x0::composite_buffer::chunk& ch);

inline std::ostream& operator<<(std::ostream& os, const x0::composite_buffer& cb)
{
	for (auto i = cb.begin(), e = cb.end(); i != e; ++i)
	{
		os << *i;
	}

	return os;
}

inline std::ostream& operator<<(std::ostream& os, const x0::composite_buffer::chunk& ch)
{
	switch (ch.type())
	{
		case x0::composite_buffer::chunk::ciov:
			{
				const x0::composite_buffer::iovec_chunk& iov = *static_cast<const x0::composite_buffer::iovec_chunk *>(&ch);
				for (int i = 0, e = iov.length(); i != e; ++i)
				{
					os.write(static_cast<const char *>(iov[i].iov_base), iov[i].iov_len);
				}
			}
			break;
		case x0::composite_buffer::chunk::cfd:
			{
				const x0::composite_buffer::fd_chunk& fdc = *static_cast<const x0::composite_buffer::fd_chunk *>(&ch);

				if (scoped_mmap map = scoped_mmap(NULL, fdc.size(), PROT_READ, MAP_PRIVATE, fdc.fd(), 0))
				{
					os.write(map.address<char>() + fdc.offset(), fdc.size());
				}
			}
			break;
		default:
			break;
	}

	return os;
}

#endif
