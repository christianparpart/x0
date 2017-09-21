// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging/FileLogTarget.h>
#include <xzero/logging/LogLevel.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/WallClock.h>
#include <xzero/UnixTime.h>
#include <unistd.h>

namespace xzero {

FileLogTarget::FileLogTarget(FileDescriptor&& fd)
    : fd_(std::move(fd)),
      timestampEnabled_(true) {
}

// TODO is a mutex required for concurrent printf()'s ?
void FileLogTarget::log(LogLevel level,
                           const std::string& component,
                           const std::string& message) {
  std::string logline = StringUtil::format(
      "$0[$1] [$2] $3\n",
      createTimestamp(),
      level,
      component,
      message);

  FileUtil::write(fd_, logline);
}

std::string FileLogTarget::createTimestamp() const {
  if (timestampEnabled_ == false)
    return "";

  return WallClock::now().toString("%Y-%m-%d %H:%M:%S ");
}

} // namespace xzero

