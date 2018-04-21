// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileHandle.h>
#include <xzero/Buffer.h>
#include <xzero/defines.h>
#include <cstdint>

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

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
class FileView {
 private:
  FileView(const FileView&) = delete;
  FileView& operator=(const FileView&) = delete;

 public:
  /**
   * Initializes given FileView.
   *
   * @param fd Underlying resource file descriptor.
   * @param offset The offset to start reading from.
   * @param size Number of bytes to read.
   */
  FileView(FileHandle&& fd, off_t offset, size_t size);

  /**
   * Initializes given FileView.
   *
   * @param fd Underlying resource file descriptor.
   * @param offset The offset to start reading from.
   * @param size Number of bytes to read.
   */
  FileView(const FileHandle& fd, off_t offset, size_t size);

  /** General move semantics for FileView(FileView&&). */
  FileView(FileView&& other);

  /** Safely closes the underlying resource file descriptor. */
  ~FileView();

  /** Moves the FileView @p other into @c *this. */
  FileView& operator=(FileView&& other);

  FileHandle& handle() noexcept { return fd_; }
  const FileHandle& handle() const noexcept { return fd_; }

  off_t offset() const noexcept { return offset_; }
  void setOffset(off_t n) { offset_ = n; }

  bool empty() const noexcept { return size_ == 0; }
  size_t size() const noexcept { return size_; }
  void setSize(size_t n) { size_ = n; }

  void read(Buffer* output) const;

  FileView view(size_t offset, size_t n) const;

  FileHandle&& release() {
    close_ = false;
    return std::move(fd_);
  }

 private:
  FileHandle fd_;
  off_t offset_;
  size_t size_;
  bool close_;
};

// {{{ inlines
inline FileView::FileView(FileHandle&& fd, off_t offset, size_t size)
    : fd_(fd.release()),
      offset_(offset),
      size_(size),
      close_(true) {
}

inline FileView::FileView(const FileHandle& fd, off_t offset, size_t size)
    : fd_(fd.native()),
      offset_(offset),
      size_(size),
      close_(false) {
}

inline FileView::FileView(FileView&& ref)
    : fd_(std::move(ref.fd_)),
      offset_(ref.offset_),
      size_(ref.size_),
      close_(ref.close_) {
  ref.close_ = false;
}

inline FileView::~FileView() {
  if (!close_) {
    fd_.release();
  }
}

inline FileView& FileView::operator=(FileView&& ref) {
  fd_ = std::move(ref.fd_);
  offset_ = ref.offset_;
  size_ = ref.size_;
  close_ = ref.close_;

  ref.close_ = false;

  return *this;
}

inline FileView FileView::view(size_t offset, size_t n) const {
  return FileView(fd_,
                  offset_ + offset,
                  std::min(n, size_ - offset));
}

} // namespace xzero
