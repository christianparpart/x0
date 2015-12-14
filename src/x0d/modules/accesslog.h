// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include "XzeroModule.h"
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
