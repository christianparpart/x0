// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Value.h>

namespace x0 {

class X0_API Constant : public Value {
 public:
  Constant(FlowType ty, const std::string& name) : Value(ty, name) {}

  void dump() override;
};

}  // namespace x0
