// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/UnixTime.h>
#include <xzero/io/File.h>
#include <xzero/io/FileHandle.h>

#include <string>

namespace xzero {

class MemoryFile : public File {
 public:
  /** Initializes a "not found" file. */
  MemoryFile();

  /** Initializes a memory backed file. */
  MemoryFile(const std::string& path,
             const std::string& mimetype,
             const BufferRef& data,
             UnixTime mtime);

  ~MemoryFile();

  const std::string& etag() const override;
  size_t size() const noexcept override;
  UnixTime mtime() const noexcept override;
  size_t inode() const noexcept override;
  bool isRegular() const noexcept override;
  bool isDirectory() const noexcept override;
  bool isExecutable() const noexcept override;
  FileHandle createPosixChannel(FileOpenFlags flags) override;

 private:
  UnixTime mtime_;
  size_t inode_;
  size_t size_;
  std::string etag_;
  std::string fspath_;
  FileHandle fd_;
};

} // namespace xzero

