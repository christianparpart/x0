// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/FileRepository.h>
#include <functional>
#include <string>

namespace xzero {

class MimeTypes;
class LocalFile;

class XZERO_BASE_API LocalFileRepository : public FileRepository {
 public:
  /**
   * Initializes local file repository.
   *
   * @param mimetypes mimetypes database to use for creating entity tags.
   * @param basedir base directory to start all lookups from (like "/").
   * @param etagMtime whether or not to include Last-Modified timestamp in etag.
   * @param etagSize whether or not to include file size in etag.
   * @param etagInode whether or not to include file's system inode in etag.
   */
  LocalFileRepository(
      MimeTypes& mimetypes,
      const std::string& basedir,
      bool etagMtime, bool etagSize, bool etagInode);

  const std::string baseDirectory() const { return basedir_; }

  std::shared_ptr<File> getFile(
      const std::string& requestPath,
      const std::string& docroot = "/") override;

  void listFiles(std::function<bool(const std::string&)> callback) override;
  void deleteAllFiles() override;
  int createTempFile(std::string* filename = nullptr) override;

  /**
   * Configures ETag generation.
   *
   * @param mtime whether or not to include Last-Modified timestamp
   * @param size whether or not to include file size
   * @param inode whether or not to include file's system inode
   */
  void configureETag(bool mtime, bool size, bool inode);

  bool etagConsiderMTime() const noexcept { return etagConsiderMTime_; }
  bool etagConsiderSize() const noexcept { return etagConsiderSize_; }
  bool etagConsiderINode() const noexcept { return etagConsiderINode_; }

 private:
  friend class LocalFile;

  MimeTypes& mimetypes_;
  std::string basedir_;
  bool etagConsiderMTime_;
  bool etagConsiderSize_;
  bool etagConsiderINode_;
};

}  // namespace xzero

