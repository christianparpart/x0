/* <MemoryPool.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_flow_pool_h
#define sw_flow_pool_h (1)

#include <list>
#include <cstdlib>
#include <cstring>

/*! pool-based memory manager.
 *
 * That is, you can easily free all allocated resources at pool's destruction
 * or explicitely at your own will.
 */
class MemoryPool
{
public:
	std::list<void *> pool_;

public:
	MemoryPool();
	~MemoryPool();

	void clear();

	void *allocate(size_t n);

	template<typename T>
	T* allocate()
	{
		return static_cast<T*>(allocate(sizeof(T)));
	}

	char *strdup(const char *value);
	char *strcat(const char *v1, const char *v2);

	void *dup(const void *val, size_t size);
	void *cat(const void *v1, size_t s1, const void *v2, size_t s2);
};

// {{{ inlines
inline MemoryPool::MemoryPool()
{
}

inline MemoryPool::~MemoryPool()
{
	clear();
}

inline void MemoryPool::clear()
{
	for (auto i = pool_.begin(), e = pool_.end(); i != e; ++i)
		free(*i);

	pool_.clear();
}

inline void *MemoryPool::allocate(size_t n)
{
	void *result = malloc(n);
	pool_.push_back(result);
	return result;
}

inline char *MemoryPool::strdup(const char *value)
{
	size_t n = strlen(value);
	char *result = (char *)(allocate(n + 1));
	memcpy(result, value, n + 1);
	return result;
}

inline char *MemoryPool::strcat(const char *v1, const char *v2)
{
	size_t n1 = strlen(v1);
	size_t n2 = strlen(v2);

	char *result = (char *)allocate(n1 + n2 + 1);

	memcpy(result, v1, n1);
	memcpy(result + n1, v2, n2 + 1);

	return result;
}
// }}}

#endif
