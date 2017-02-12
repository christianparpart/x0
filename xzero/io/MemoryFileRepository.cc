// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/MemoryFileRepository.h>
#include <xzero/RuntimeError.h>
#include <xzero/MimeTypes.h>

namespace xzero {

MemoryFileRepository::MemoryFileRepository(MimeTypes& mimetypes)
    : mimetypes_(mimetypes),
      files_(),
      notFound_(new MemoryFile()) {
}

std::shared_ptr<File> MemoryFileRepository::getFile(
    const std::string& requestPath,
    const std::string& /*docroot*/) {
  auto i = files_.find(requestPath);
  if (i != files_.end())
    return i->second;

  return notFound_;
}

void MemoryFileRepository::listFiles(
    std::function<bool(const std::string&)> callback) {
  for (auto& file: files_) {
    if (!callback(file.first)) {
      break;
    }
  }
}

void MemoryFileRepository::deleteAllFiles() {
  files_.clear();
}

int MemoryFileRepository::createTempFile(std::string* filename) {
  RAISE_STATUS(NotImplementedError);
}

void MemoryFileRepository::insert(
    const std::string& path, const BufferRef& data, UnixTime mtime) {
  files_[path].reset(new MemoryFile(path,
                                    mimetypes_.getMimeType(path),
                                    data,
                                    mtime));
}

void MemoryFileRepository::insert(
    const std::string& path, const BufferRef& data) {
  insert(path, data, UnixTime());
}

} // namespace xzero
