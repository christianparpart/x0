// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/ir/Constant.h>
#include <xzero-flow/vm/Signature.h>

namespace xzero {
namespace flow {

class XZERO_FLOW_API IRBuiltinFunction : public Constant {
 public:
  IRBuiltinFunction(const vm::Signature& sig)
      : Constant(sig.returnType(), sig.name()), signature_(sig) {}

  const vm::Signature& signature() const { return signature_; }
  const vm::Signature& get() const { return signature_; }

 private:
  vm::Signature signature_;
};

}  // namespace flow
}  // namespace xzero
