// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Module.h>

namespace x0d {

class AccessModule : public Module {
 public:
  explicit AccessModule(Daemon* d);

 private:
  // deny()
  bool deny_all(Context* cx, Params& args);
  bool deny_ip(Context* cx, Params& args);
  bool deny_cidr(Context* cx, Params& args);
  bool deny_ipArray(Context* cx, Params& args);
  bool deny_cidrArray(Context* cx, Params& args);

  // deny_except()
  bool denyExcept_ip(Context* cx, Params& args);
  bool denyExcept_cidr(Context* cx, Params& args);
  bool denyExcept_ipArray(Context* cx, Params& args);
  bool denyExcept_cidrArray(Context* cx, Params& args);

  bool forbidden(Context* cx);
};

} // namespace x0d
