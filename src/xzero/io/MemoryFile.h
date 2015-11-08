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
#include <xzero/UnixTime.h>
#include <string>

namespace xzero {

class XZERO_BASE_API MemoryFile : public File {
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
  size_t size() const XZERO_NOEXCEPT override;
  time_t mtime() const XZERO_NOEXCEPT override;
  size_t inode() const XZERO_NOEXCEPT override;
  bool isRegular() const XZERO_NOEXCEPT override;
  bool isDirectory() const XZERO_NOEXCEPT override;
  bool isExecutable() const XZERO_NOEXCEPT override;
  int createPosixChannel(OpenFlags flags) override;
  std::unique_ptr<InputStream> createInputChannel() override;
  std::unique_ptr<OutputStream> createOutputChannel() override;
  std::unique_ptr<MemoryMap> createMemoryMap(bool rw = true) override;

 private:
  time_t mtime_;
  size_t inode_;
  size_t size_;
  std::string etag_;
  std::string fspath_;
  int fd_;
};

} // namespace xzero

