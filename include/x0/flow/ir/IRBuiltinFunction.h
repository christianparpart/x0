// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Constant.h>
#include <x0/flow/vm/Signature.h>

namespace x0 {

class X0_API IRBuiltinFunction : public Constant {
 public:
  IRBuiltinFunction(const FlowVM::Signature& sig)
      : Constant(sig.returnType(), sig.name()), signature_(sig) {}

  const FlowVM::Signature& signature() const { return signature_; }
  const FlowVM::Signature& get() const { return signature_; }

 private:
  FlowVM::Signature signature_;
};

}  // namespace x0
