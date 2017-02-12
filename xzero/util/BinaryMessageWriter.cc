// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/util/BinaryMessageWriter.h>
#include <xzero/RuntimeError.h>
#include <stdlib.h>
#include <string.h>

namespace xzero {
namespace util {

BinaryMessageWriter::BinaryMessageWriter(
    size_t initial_size) :
    ptr_(malloc(initial_size)),
    size_(initial_size),
    used_(0),
    owned_(true) {
  if (ptr_ == nullptr) {
    RAISE(MallocError);
  }
}

BinaryMessageWriter::BinaryMessageWriter(
    void* buf,
    size_t buf_len) :
    ptr_(buf),
    size_(buf_len),
    used_(0),
    owned_(false) {}

BinaryMessageWriter::~BinaryMessageWriter() {
  if (owned_) {
    free(ptr_);
  }
}

void BinaryMessageWriter::appendUInt16(uint16_t value) {
  append(&value, sizeof(value));
}

void BinaryMessageWriter::updateUInt16(size_t offset, uint16_t value) {
  update(offset, &value, sizeof(value));
}

void BinaryMessageWriter::appendUInt32(uint32_t value) {
  append(&value, sizeof(value));
}

void BinaryMessageWriter::updateUInt32(size_t offset, uint32_t value) {
  update(offset, &value, sizeof(value));
}

void BinaryMessageWriter::appendUInt64(uint64_t value) {
  append(&value, sizeof(value));
}

void BinaryMessageWriter::updateUInt64(size_t offset, uint64_t value) {
  update(offset, &value, sizeof(value));
}

void BinaryMessageWriter::appendString(const std::string& string) {
  append(string.data(), string.size());
}

void BinaryMessageWriter::updateString(
    size_t offset,
    const std::string& string) {
  update(offset, string.data(), string.size());
}

void* BinaryMessageWriter::data() const {
  return ptr_;
}

size_t BinaryMessageWriter::size() const {
  return used_;
}

void BinaryMessageWriter::append(void const* data, size_t size) {
  size_t resize = size_;

  while (used_ + size > resize) {
    resize *= 2;
  }

  if (resize > size_) {
    if (!owned_) {
      RAISE(BufferOverflowError);//, "provided buffer is too small");
    }

    auto new_ptr = realloc(ptr_, resize);

    if (ptr_ == nullptr) {
      RAISE(MallocError);//, "realloc() failed");
    }

    ptr_ = new_ptr;
  }

  memcpy(((char*) ptr_) + used_, data, size);
  used_ += size;
}

void BinaryMessageWriter::update(size_t offset, void const* data, size_t size) {
  if (offset + size > size_) {
    RAISE(BufferOverflowError);//, "update exceeds buffer boundary");
  }

  memcpy(((char*) ptr_) + offset, data, size);
}

} // namespace util
} // namespace xzero
