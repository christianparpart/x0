// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
 *
 * It first attempts to store the data in memory, with
 * up to @c n bytes, as specified in the constructor.
 *
 * Once this threshold is being exceeded, the buffer is
 * swapped onto a temporary disk location and further bytes
 * will be written to disk, too.
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

  void reset();

 private:
  void tryDisplaceBufferToFile();

 private:
  size_t maxBufferSize_;
  size_t actualSize_;
  Buffer buffer_;
  FileDescriptor fd_;
};

} // namespace http
