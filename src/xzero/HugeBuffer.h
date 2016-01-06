// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/io/FileView.h>
#include <xzero/io/FileDescriptor.h>
#include <memory>

namespace xzero {

class InputStream;

/**
 * A huge buffer API that can contain more data than your RAM.
 */
class HugeBuffer {
 public:
  explicit HugeBuffer(size_t maxBufferSize);

  bool empty() const noexcept { return actualSize_ == 0; }
  size_t size() const noexcept { return actualSize_; }
  bool isFile() const noexcept { return fd_.isOpen(); }

  void write(const BufferRef& chunk);
  void write(const FileView& chunk);
  void write(FileView&& chunk);
  void write(Buffer&& chunk);

  FileView getFileView() const;
  const BufferRef& getBuffer() const;
  std::unique_ptr<InputStream> getInputStream();

  void tryDisplaceBufferToFile();

  void reset();

 private:
  size_t maxBufferSize_;
  size_t actualSize_;
  Buffer buffer_;
  FileDescriptor fd_;
};

} // namespace http
