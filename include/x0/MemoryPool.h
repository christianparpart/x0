/* <x0/MemoryPool.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_MemoryPool_hpp
#define sw_x0_MemoryPool_hpp

#include <x0/Api.h>
#include <cstddef>
#include <vector>

namespace x0 {

//! \addtogroup base
//@{

class MemoryPool
{
private:
	std::vector<void *> pool_;
	size_t chunkSize_;
	size_t bytesAvalable_;

public:
	explicit MemoryPool(size_t chunkSize = 4096);
	~MemoryPool();

	void *allocate(size_t size);
	void clear();
};

//@}

} // namespace x0

#endif
