// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_cache_CacheService_h
#define sw_x0_cache_CacheService_h

#include <base/Api.h>
#include <base/Buffer.h>
#include <string.h>

namespace base {

class BASE_API CacheService {
 public:
  virtual ~CacheService();

  virtual bool set(const char* key, const char* value);
  virtual bool set(const BufferRef& key, const BufferRef& value);
  virtual bool set(const char* key, size_t keysize, const char* val,
                   size_t valsize) = 0;

  virtual bool get(const char* key, Buffer& value);
  virtual bool get(const BufferRef& key, Buffer& value);
  virtual bool get(const char* key, size_t keysize, Buffer& val) = 0;
};

}  // namespace base

#endif
