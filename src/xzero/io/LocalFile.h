// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
  std::unique_ptr<std::istream> createInputChannel() override;
  std::unique_ptr<std::ostream> createOutputChannel() override;
  std::unique_ptr<MemoryMap> createMemoryMap(bool rw = true) override;

  void update();

 private:
  LocalFileRepository& repo_;
  struct stat stat_;
  mutable std::string etag_;
};

} // namespace xzero

