#ifndef _sw_scoped_mmap_hpp
#define _sw_scoped_mmap_hpp 1

#include <sys/mman.h>

/**
 * \ingroup detail
 * \brief scoped memory-map helper class.
 *
 * Invokes syscall mmap on constructor and cleanly invokes munmap on destruction.
 */
class scoped_mmap
{
private:
	void *ptr;
	int size;

public:
	scoped_mmap(void *addr, std::size_t length, int prot, int flags, int fd, off_t offset)
	:
		ptr(::mmap(addr, length, prot, flags, fd, offset)),
		size(length)
	{
	}

	~scoped_mmap()
	{
		::munmap(ptr, size);
	}

	operator bool() const
	{
		return ptr != MAP_FAILED;
	}

	template<class T = void> T *address() const
	{
		return static_cast<T *>(ptr);
	}
};

#endif
