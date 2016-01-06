// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "stx/logging.h"
#include "stx/logging/logoutputstream.h"
#include "stx/stringutil.h"
#include "stx/wallclock.h"

using stx::OutputStream;

namespace stx {

LogOutputStream::LogOutputStream(
    std::unique_ptr<OutputStream> target) :
    target_(std::move(target)) {}

void LogOutputStream::log(
    LogLevel level,
    const String& component,
    const String& message) {
  const auto prefix = StringUtil::format(
      "$0 $1 [$2] ",
      WallClock::now().toString("%Y-%m-%d %H:%M:%S"),
      StringUtil::toString(level).c_str(),
      component);

  std::string lines = prefix + message;
  StringUtil::replaceAll(&lines, "\n", "\n" + prefix);
  lines.append("\n");

  ScopedLock<std::mutex> lk(target_->mutex);
  target_->write(lines);
}

}
