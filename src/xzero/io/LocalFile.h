// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/File.h>
#include <string>
#include <functional>
#include <unordered_map>
#include <sys/stat.h>

namespace xzero {

class LocalFileRepository;

class LocalFile : public File {
 public:
  LocalFile(LocalFileRepository& repo,
            const std::string& path,
            const std::string& mimetype);
  ~LocalFile();

  static std::shared_ptr<LocalFile> get(const std::string& path);

  const std::string& etag() const override;
  size_t size() const noexcept override;
  UnixTime mtime() const noexcept override;
  size_t inode() const noexcept override;
  bool isRegular() const noexcept override;
  bool isDirectory() const noexcept override;
  bool isExecutable() const noexcept override;
  int createPosixChannel(OpenFlags flags) override;

  void update();

 private:
  LocalFileRepository& repo_;
  struct stat stat_;
  mutable std::string etag_;
};

} // namespace xzero

