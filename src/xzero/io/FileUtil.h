// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/io/FileHandle.h>
#include <xzero/Result.h>
#include <functional>
#include <string>
#include <stdint.h>

namespace xzero {

class Buffer;
class BufferRef;
class File;
class FileView;

class FileUtil {
 public:
  static char pathSeperator() noexcept;

  static std::string currentWorkingDirectory();

  static std::string absolutePath(const std::string& relpath);
  static Result<std::string> realpath(const std::string& relpath);
  static bool exists(const std::string& path);
  static bool isDirectory(const std::string& path);
  static bool isRegular(const std::string& path);
  static uintmax_t size(const std::string& path);
  static uintmax_t sizeRecursive(const std::string& path);
  static void ls(const std::string& path, std::function<bool(const std::string&)> cb);

  static std::string joinPaths(const std::string& base, const std::string& append);

  static void seek(int fd, off_t offset);
  static size_t read(int fd, Buffer* output);
  static size_t read(File&, Buffer* output);
  static size_t read(const std::string& path, Buffer* output);
  static size_t read(const FileView& file, Buffer* output);
  static Buffer read(FileHandle& fd);
  static Buffer read(File& file);
  static Buffer read(const FileView& file);
  static Buffer read(const std::string& path);
  static void write(const std::string& path, const BufferRef& buffer);
  static void write(const std::string& path, const std::string& buffer);
  static void write(FileHandle& fd, const char* cstr);
  static void write(FileHandle& fd, const BufferRef& chunk);
  static void write(FileHandle& fd, const std::string& chunk);
  static void write(FileHandle& fd, const FileView& chunk);
  static void copy(const std::string& from, const std::string& to);
  static void truncate(const std::string& path, size_t size);
  static std::string dirname(const std::string& path);
  static std::string basename(const std::string& path);
  static void mkdir_p(const std::string& path);
  static void rm(const std::string& path);
  static void mv(const std::string& path, const std::string& target);
  static void chown(const std::string& path,
                    const std::string& user,
                    const std::string& group);

  static FileHandle createTempFile();
  static FileHandle createTempFileAt(const std::string& basedir,
                                     std::string* result = nullptr);
  static std::string tempDirectory();

  static void close(int fd);
};

}  // namespace xzero
