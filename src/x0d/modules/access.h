// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroModule.h>

namespace x0d {

class AccessModule : public XzeroModule {
 public:
  explicit AccessModule(XzeroDaemon* d);

 private:
  // deny()
  bool deny_all(XzeroContext* cx, Params& args);
  bool deny_ip(XzeroContext* cx, Params& args);
  bool deny_cidr(XzeroContext* cx, Params& args);
  bool deny_ipArray(XzeroContext* cx, Params& args);
  bool deny_cidrArray(XzeroContext* cx, Params& args);

  // deny_except()
  bool denyExcept_ip(XzeroContext* cx, Params& args);
  bool denyExcept_cidr(XzeroContext* cx, Params& args);
  bool denyExcept_ipArray(XzeroContext* cx, Params& args);
  bool denyExcept_cidrArray(XzeroContext* cx, Params& args);

  bool forbidden(XzeroContext* cx);
};

} // namespace x0d
