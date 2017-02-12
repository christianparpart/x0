// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroModule.h>
#include <xzero/io/File.h>
#include <xzero/io/LocalFile.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/io/OutputStream.h>
#include <xzero/Option.h>
#include <ostream>

namespace x0d {

class LogFile {
 public:
  explicit LogFile(std::shared_ptr<xzero::File> file);
  ~LogFile();

  void write(xzero::Buffer&& message);
  void cycle();

 private:
  std::shared_ptr<xzero::File> file_;
  std::unique_ptr<xzero::OutputStream> output_;
};

class AccesslogModule : public XzeroModule {
 public:
  explicit AccesslogModule(XzeroDaemon* d);
  ~AccesslogModule() override;

 private:
  void accesslog_format(Params& args);

  void accesslog_syslog(XzeroContext* cx, Params& args);
  void accesslog_file(XzeroContext* cx, Params& args);

  using FlowString = xzero::flow::FlowString;

  xzero::Option<FlowString> lookupFormat(const FlowString& id) const;
  LogFile* getLogFile(const FlowString& filename);

  void onCycle();

 private:
  typedef std::unordered_map<std::string, std::shared_ptr<LogFile>> LogMap;

  std::unordered_map<FlowString, FlowString> formats_;
  LogMap logfiles_;  // map of file's name-to-fd
};

} // namespace x0d
