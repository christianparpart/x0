#ifndef sw_flow_pool_h
#define sw_flow_pool_h (1)

#include <list>
#include <cstdlib>
#include <cstring>

class PoolMgr
{
public:
	std::list<void *> pool_;

public:
	PoolMgr();
	~PoolMgr();

	void clear();

	void *allocate(size_t n);

	char *strdup(const char *value);
	char *strcat(const char *v1, const char *v2);

	void *dup(const void *val, size_t size);
	void *cat(const void *v1, size_t s1, const void *v2, size_t s2);
};

// {{{ inlines
PoolMgr::PoolMgr()
{
}

PoolMgr::~PoolMgr()
{
	clear();
}

void PoolMgr::clear()
{
	for (auto i = pool_.begin(), e = pool_.end(); i != e; ++i)
		free(*i);

	pool_.clear();
}

void *PoolMgr::allocate(size_t n)
{
	void *result = malloc(n);
	pool_.push_back(result);
	return result;
}

char *PoolMgr::strdup(const char *value)
{
	size_t n = strlen(value);
	char *result = (char *)(allocate(n + 1));
	memcpy(result, value, n + 1);
	return result;
}

char *PoolMgr::strcat(const char *v1, const char *v2)
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
