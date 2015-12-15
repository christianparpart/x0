// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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
