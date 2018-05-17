// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Module.h>
#include <xzero/io/File.h>
#include <xzero/io/FileDescriptor.h>
#include <optional>
#include <ostream>

namespace flow {
  class Instr;
  class IRBuilder;
};

namespace x0d {

class LogFile {
 public:
  explicit LogFile(std::shared_ptr<xzero::File> file);
  ~LogFile();

  void write(xzero::Buffer&& message);
  void cycle();

 private:
  std::shared_ptr<xzero::File> file_;
  xzero::FileHandle fd_;
};

class AccesslogFormatError : public ConfigurationError {
 public:
  explicit AccesslogFormatError(const std::string& msg)
      : ConfigurationError{"accesslog format error. " + msg}
  {}
};

class AccesslogModule : public Module {
 public:
  explicit AccesslogModule(Daemon* d);
  ~AccesslogModule() override;

 private:
  void accesslog_format(Params& args);
  bool accesslog_format_verifier(flow::Instr* call,
                                 flow::IRBuilder* builder);

  void accesslog_syslog(Context* cx, Params& args);
  void accesslog_console(Context* cx, Params& args);
  void accesslog_file(Context* cx, Params& args);

  using FlowString = flow::FlowString;

  std::optional<FlowString> lookupFormat(const FlowString& id) const;
  LogFile* getLogFile(const FlowString& filename);

  void onCycle();

 private:
  typedef std::unordered_map<std::string, std::shared_ptr<LogFile>> LogMap;

  std::unordered_map<FlowString, FlowString> formats_;
  LogMap logfiles_;  // map of file's name-to-fd
};

} // namespace x0d
