// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <x0d/XzeroModule.h>

namespace x0d {

class AuthModule : public XzeroModule {
 public:
  explicit AuthModule(XzeroDaemon* d);
  ~AuthModule();

  // main functions
  void auth_realm(XzeroContext* cx, Params& args);
  void auth_userfile(XzeroContext* cx, Params& args);
  void auth_pam(XzeroContext* cx, Params& args);

  // main handlers
  bool auth_require(XzeroContext* cx, Params& args);

  bool sendAuthenticateRequest(XzeroContext* cx, const std::string& realm);
};

} // namespace x0d
