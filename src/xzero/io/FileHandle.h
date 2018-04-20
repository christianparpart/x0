// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>

#if defined(XZERO_OS_WINDOWS)
#include <Windows.h>
#endif

#if defined(XZERO_OS_UNIX)
#include <unistd.h>
#endif

namespace xzero {

/**
  * Represents a system file descriptor that gets automatically closed.
  */
class[[nodiscard]] FileHandle{
 public:
#if defined(XZERO_OS_WINDOWS)
  using native_type = HANDLE;
  static inline const native_type InvalidHandle = INVALID_HANDLE_VALUE;
#else
  using native_type = int;
  static const native_type InvalidHandle = -1;
#endif

  FileHandle(const FileHandle& other) = delete;
  FileHandle& operator=(const FileHandle& other) = delete;

  FileHandle() : handle_{InvalidHandle} {}
  explicit FileHandle(native_type handle) : handle_{handle} {}
  ~FileHandle() { close(); }

  FileHandle(FileHandle&& other) : handle_{other.release()} {}
  FileHandle& operator=(FileHandle&& other) {
    close();
    handle_ = other.release();
    return *this;
  }

  bool isClosed() const noexcept { return handle_ == InvalidHandle; }
  bool isOpen() const noexcept { return handle_ != InvalidHandle; }

  native_type native() const noexcept { return handle_; }
  operator native_type() const noexcept { return handle_; }

  native_type release() {
    native_type n = handle_;
    handle_ = InvalidHandle;
    return n;
  }

  void close();

  ssize_t write(const void* buf, size_t count);

 private:
  native_type handle_;
};

inline void FileHandle::close() {
  if (isOpen()) {
#if defined(XZERO_OS_WINDOWS)
    CloseHandle(release());
#else
    ::close(release());
#endif
  }
}

#if defined(XZERO_OS_WINDOWS)
inline ssize_t FileHandle::write(const void* buf, size_t count) {
  DWORD nwritten = 0;
  if (!WriteFile(handle_, buf, count, &nwritten, nullptr))
    return -1;
  return static_cast<ssize_t>(nwritten);
}
#endif

#if defined(XZERO_OS_UNIX)
inline ssize_t FileHandle::write(const void* buf, size_t count) {
  return ::write(handle_, buf, count);
}
#endif


}  // namespace xzero
