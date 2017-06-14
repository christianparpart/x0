// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
