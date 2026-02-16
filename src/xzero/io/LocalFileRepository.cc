// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/LocalFileRepository.h>
#include <xzero/io/LocalFile.h>
#include <xzero/io/FileUtil.h>
#include <xzero/MimeTypes.h>

namespace xzero {

LocalFileRepository::LocalFileRepository(MimeTypes& mt,
                                                 const std::string& basedir,
                                                 bool etagMtime,
                                                 bool etagSize,
                                                 bool etagInode)
    : mimetypes_(mt),
      basedir_(*FileUtil::realpath(basedir)),
      etagConsiderMTime_(etagMtime),
      etagConsiderSize_(etagSize),
      etagConsiderINode_(etagInode) {
}

std::shared_ptr<File> LocalFileRepository::getFile(const std::string& requestPath) {
    return std::make_unique<LocalFile>(*this,
                                       FileUtil::joinPaths(basedir_, requestPath),
                                       mimetypes_.getMimeType(requestPath));
}

void LocalFileRepository::listFiles(
    std::function<bool(const std::string&)> callback) {
  FileUtil::ls(basedir_,
               [&](const std::string& filename) -> bool {
                  return callback(FileUtil::joinPaths(basedir_, filename));
               });
}

void LocalFileRepository::deleteAllFiles() {
  FileUtil::ls(basedir_,
               [&](const std::string& filename) -> bool {
                  FileUtil::rm(FileUtil::joinPaths(basedir_, filename));
                  return true;
               });
}

FileHandle LocalFileRepository::createTempFile(std::string* filename) {
  if (basedir_.empty() || basedir_ == "/")
    return FileUtil::createTempFileAt(FileUtil::tempDirectory(), filename);
  else
    return FileUtil::createTempFileAt(basedir_, filename);
}

void LocalFileRepository::configureETag(bool mtime, bool size, bool inode) {
  etagConsiderMTime_ = mtime;
  etagConsiderSize_ = size;
  etagConsiderINode_ = inode;
}

}  // namespace xzero
