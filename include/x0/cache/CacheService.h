/* <CacheService.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_cache_CacheService_h
#define sw_x0_cache_CacheService_h

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <string.h>

namespace x0 {

class X0_API CacheService
{
public:
	virtual ~CacheService();

	virtual bool set(const char* key, const char* value);
	virtual bool set(const BufferRef& key, const BufferRef& value);
	virtual bool set(const char* key, size_t keysize, const char* val, size_t valsize) = 0;

	virtual bool get(const char* key, Buffer& value);
	virtual bool get(const BufferRef& key, Buffer& value);
	virtual bool get(const char* key, size_t keysize, Buffer& val) = 0;
};

} // namespace x0

#endif
