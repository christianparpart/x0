// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/io/FileHandle.h>
#include <functional>
#include <string>
#include <memory>

namespace xzero {

class File;

class FileRepository {
 public:
  virtual ~FileRepository();

  virtual std::shared_ptr<File> getFile(const std::string& requestPath) = 0;

  virtual void listFiles(std::function<bool(const std::string&)> callback) = 0;
  virtual void deleteAllFiles() = 0;

  virtual FileHandle createTempFile(std::string* filename = nullptr) = 0;
};

}  // namespace xzero
