// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/Buffer.h>
#include <cstdint>
#include <unistd.h>

namespace xzero {

/**
 * Basic abstraction of an open file handle, its range, and auto-close feature.
 *
 * This FileView represents an open file that is to be read starting
 * from the given @c offset up to @c size bytes.
 *
 * If the FileView was initialized with auto-close set to on, its
 * underlying resource file descriptor will be automatically closed.
 */
class XZERO_BASE_API FileView {
 private:
  FileView(const FileView&) = delete;
  FileView& operator=(const FileView&) = delete;

 public:
  /** General move semantics for FileView(FileView&&). */
  FileView(FileView&& ref)
      : fd_(ref.fd_),
        offset_(ref.offset_),
        size_(ref.size_),
        close_(ref.close_) {
    ref.fd_ = -1;
    ref.close_ = false;
  }

  /** General move semantics for operator=. */
  FileView& operator=(FileView&& ref) {
    fd_ = ref.fd_;
    offset_ = ref.offset_;
    size_ = ref.size_;
    close_ = ref.close_;

    ref.fd_ = -1;
    ref.close_ = false;

    return *this;
  }

  /**
   * Initializes given FileView.
   *
   * @param fd Underlying resource file descriptor.
   * @param offset The offset to start reading from.
   * @param size Number of bytes to read.
   * @param close Whether or not to close the underlying file desriptor upon
   *              object destruction.
   */
  FileView(int fd, off_t offset, size_t size, bool close)
      : fd_(fd), offset_(offset), size_(size), close_(close) {}

  /**
   * Initializes given FileView.
   *
   * @param fd Underlying resource file descriptor.
   * @param offset The offset to start reading from.
   * @param size Number of bytes to read.
   */
  FileView(FileDescriptor&& fd, off_t offset, size_t size)
      : fd_(fd.release()), offset_(offset), size_(size), close_(true) {}

  /**
   * Conditionally closes the underlying resource file descriptor.
   */
  ~FileView() {
    if (close_) {
      FileUtil::close(fd_);
    }
  }

  int release() noexcept { close_ = false; return fd_; }

  int handle() const noexcept { return fd_; }

  off_t offset() const noexcept { return offset_; }
  void setOffset(off_t n) { offset_ = n; }

  bool empty() const noexcept { return size_ == 0; }
  size_t size() const noexcept { return size_; }
  void setSize(size_t n) { size_ = n; }

  void fill(Buffer* output) const;

  FileView view(size_t offset, size_t n) const;

 private:
  int fd_;
  off_t offset_;
  size_t size_;
  bool close_;
};

inline FileView FileView::view(size_t offset, size_t n) const {
  return FileView(fd_,
                  offset_ + offset,
                  std::min(n, size_ - offset),
                  false);
}

} // namespace xzero
