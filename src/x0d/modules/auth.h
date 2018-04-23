// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Module.h>

namespace x0d {

class AuthModule : public Module {
 public:
  explicit AuthModule(Daemon* d);
  ~AuthModule();

  // main functions
  void auth_realm(Context* cx, Params& args);
  void auth_userfile(Context* cx, Params& args);
  void auth_pam(Context* cx, Params& args);

  // main handlers
  bool auth_require(Context* cx, Params& args);

  bool sendAuthenticateRequest(Context* cx, const std::string& realm);
};

} // namespace x0d
