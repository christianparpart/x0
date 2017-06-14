// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
#include <xzero/Api.h>
#include <xzero/Buffer.h>

namespace xzero {

class XZERO_BASE_API MemoryMap : public FixedBuffer {
 public:
  MemoryMap(int fd, off_t ofs, size_t size, bool rw);
  MemoryMap(MemoryMap&& mm);
  MemoryMap(const MemoryMap& mm) = delete;
  MemoryMap& operator=(MemoryMap&& mm);
  MemoryMap& operator=(MemoryMap& mm) = delete;
  ~MemoryMap();

  bool isReadable() const;
  bool isWritable() const;

  template<typename T> T* structAt(size_t ofs) const;

 private:
  int mode_;
};

template<typename T> inline T* MemoryMap::structAt(size_t ofs) const {
  return (T*) (((char*) data_) + ofs);
}

}  // namespace xzero
