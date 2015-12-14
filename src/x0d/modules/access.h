// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include "XzeroModule.h"

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
