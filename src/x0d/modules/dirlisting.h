// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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
