// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <functional>
#include <string>
#include <stdint.h>

namespace xzero {

class Buffer;
class BufferRef;
class File;
class FileView;

class XZERO_BASE_API FileUtil {
 public:
  static char pathSeperator() noexcept;

  static std::string currentWorkingDirectory();

  static std::string absolutePath(const std::string& relpath);
  static std::string realpath(const std::string& relpath);
  static bool exists(const std::string& path);
  static bool isDirectory(const std::string& path);
  static bool isRegular(const std::string& path);
  static size_t size(const std::string& path);
  static size_t sizeRecursive(const std::string& path);
  XZERO_DEPRECATED static size_t du_c(const std::string& path) { return sizeRecursive(path); }
  static void ls(const std::string& path, std::function<bool(const std::string&)> cb);

  static std::string joinPaths(const std::string& base, const std::string& append);

  static void seek(int fd, off_t offset);
  static size_t read(int fd, Buffer* output);
  static size_t read(File&, Buffer* output);
  static size_t read(const std::string& path, Buffer* output);
  static size_t read(const FileView& file, Buffer* output);
  static Buffer read(int fd);
  static Buffer read(File& file);
  static Buffer read(const FileView& file);
  static Buffer read(const std::string& path);
  static void write(const std::string& path, const Buffer& buffer);
  static void write(int fd, const BufferRef& chunk);
  static void write(int fd, const FileView& chunk);
  static void copy(const std::string& from, const std::string& to);
  static void truncate(const std::string& path, size_t size);
  static std::string dirname(const std::string& path);
  static std::string basename(const std::string& path);
  static void mkdir(const std::string& path, int mode = 0775);
  static void mkdir_p(const std::string& path, int mode = 0775);
  static void rm(const std::string& path);
  static void mv(const std::string& path, const std::string& target);
  static void chown(const std::string& path, int uid, int gid);
  static void chown(const std::string& path,
                    const std::string& user,
                    const std::string& group);

  static int createTempFile();
  static int createTempFileAt(const std::string& basedir,
                              std::string* result = nullptr);
  static std::string createTempDirectory();
  static std::string tempDirectory();

  static void allocate(int fd, size_t length);
  static void preallocate(int fd, off_t offset, size_t length);
  static void deallocate(int fd, off_t offset, size_t length);

  /**
   * Collapses given range of a file.
   *
   * @note Operating systems that do not support it natively will be emulated.
   */
  static void collapse(int fd, off_t offset, size_t length);

  static void truncate(int fd, size_t length);

  static void close(int fd);

  static void setBlocking(int fd, bool enable);
  static bool isBlocking(int fd);
};

}  // namespace xzero
