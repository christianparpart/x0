// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/logging/LogTarget.h>
#include <xzero/io/OutputStream.h>
#include <memory>

namespace xzero {

class FileLogTarget : public LogTarget {
public:
  explicit FileLogTarget(std::unique_ptr<OutputStream>&& output);

  void log(
      LogLevel level,
      const std::string& component,
      const std::string& message) override;

  void setTimestampEnabled(bool value) { timestampEnabled_ = value; }
  bool isTimestampEnabled() const noexcept { return timestampEnabled_; }

private:
  std::string createTimestamp() const;

private:
  std::unique_ptr<OutputStream> output_;
  bool timestampEnabled_;
};

} // namespace xzero
