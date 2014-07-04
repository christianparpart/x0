// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/cache/CacheService.h>

namespace x0 {

CacheService::~CacheService() {}

bool CacheService::set(const char* key, const char* value) {
  return set(key, strlen(key), value, strlen(value));
}

bool CacheService::set(const BufferRef& key, const BufferRef& value) {
  return set(key.data(), key.size(), value.data(), value.size());
}

bool CacheService::get(const BufferRef& key, Buffer& value) {
  return get(key.data(), key.size(), value);
}

bool CacheService::get(const char* key, Buffer& value) {
  return get(key, strlen(key), value);
}

}  // namespace x0
