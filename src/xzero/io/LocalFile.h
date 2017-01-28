// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/File.h>
#include <xzero/RefPtr.h>
#include <string>
#include <functional>
#include <unordered_map>
#include <sys/stat.h>

namespace xzero {

class LocalFileRepository;

class XZERO_BASE_API LocalFile : public File {
 public:
  LocalFile(LocalFileRepository& repo,
            const std::string& path,
            const std::string& mimetype);
  ~LocalFile();

  static std::shared_ptr<LocalFile> get(const std::string& path);

  const std::string& etag() const override;
  size_t size() const XZERO_NOEXCEPT override;
  time_t mtime() const XZERO_NOEXCEPT override;
  size_t inode() const XZERO_NOEXCEPT override;
  bool isRegular() const XZERO_NOEXCEPT override;
  bool isDirectory() const XZERO_NOEXCEPT override;
  bool isExecutable() const XZERO_NOEXCEPT override;
  int createPosixChannel(OpenFlags flags) override;
  std::unique_ptr<InputStream> createInputChannel() override;
  std::unique_ptr<OutputStream> createOutputChannel(
      OpenFlags flags = File::Write | File::Create,
      int mode = 0666) override;
  std::unique_ptr<MemoryMap> createMemoryMap(bool rw = true) override;

  void update();

 private:
  LocalFileRepository& repo_;
  struct stat stat_;
  mutable std::string etag_;
};

} // namespace xzero

