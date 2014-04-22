/* <src/CacheService.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */
#include <x0/cache/CacheService.h>

namespace x0 {

CacheService::~CacheService()
{
}

bool CacheService::set(const char* key, const char* value)
{
    return set(key, strlen(key), value, strlen(value));
}

bool CacheService::set(const BufferRef& key, const BufferRef& value)
{
    return set(key.data(), key.size(), value.data(), value.size());
}

bool CacheService::get(const BufferRef& key, Buffer& value)
{
    return get(key.data(), key.size(), value);
}

bool CacheService::get(const char* key, Buffer& value)
{
    return get(key, strlen(key), value);
}

} // namespace x0
