// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include "XzeroModule.h"
#include <xzero/http/HttpOutputCompressor.h>
#include <string>

namespace x0d {

class UserdirModule : public XzeroModule {
 public:
  explicit UserdirModule(XzeroDaemon* d);
  ~UserdirModule();

 private:
  // setup properties
  void userdir_name(Params& args);
  std::error_code validate(std::string& path);

  // main functions
  void userdir(XzeroContext* cx, Params& args);

 private:
  std::string dirname_;
};

} // namespace x0d
