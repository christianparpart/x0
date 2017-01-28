// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/MemoryMap.h>
#include <xzero/RuntimeError.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace xzero {

static inline char* createMemoryMap(int fd, off_t ofs, size_t size, bool rw) {
  int prot = rw ? PROT_READ | PROT_WRITE : PROT_READ;
  char* data = (char*) mmap(nullptr, size, prot, MAP_SHARED, fd, ofs);
  if (!data)
    RAISE_ERRNO(errno);

  return data;
}

MemoryMap::MemoryMap(int fd, off_t ofs, size_t size, bool rw)
    : FixedBuffer(createMemoryMap(fd, ofs, size, rw), size, size),
      mode_(rw ? PROT_READ | PROT_WRITE : PROT_READ) {
}

MemoryMap::MemoryMap(MemoryMap&& mm)
  : FixedBuffer(mm),
    mode_(mm.mode_) {
  mm.mode_ = 0;
}

MemoryMap& MemoryMap::operator=(MemoryMap&& mm) {
  if (data_)
    munmap(data_, size_);

  data_ = mm.data_;
  mm.data_ = nullptr;

  size_ = mm.size_;
  mm.size_ = 0;

  return *this;
}

MemoryMap::~MemoryMap() {
  if (data_) {
    munmap(data_, size_);
  }
}

bool MemoryMap::isReadable() const {
  return mode_ & PROT_READ;
}

bool MemoryMap::isWritable() const {
  return mode_ & PROT_WRITE;
}

}  // namespace xzero

