// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroModule.h>
#include <xzero/io/File.h>

namespace x0d {

class DirlistingModule : public XzeroModule {
 public:
  explicit DirlistingModule(XzeroDaemon* d);

  // main handlers
  bool dirlisting(XzeroContext* cx, Params& args);

  void appendHeader(xzero::Buffer& os, const std::string& path);
  void appendDirectory(xzero::Buffer& os, const std::string& filename);
  void appendFile(xzero::Buffer& os, std::shared_ptr<xzero::File> file);
  void appendTrailer(xzero::Buffer& os);
};

} // namespace x0d
