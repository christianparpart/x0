// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/ir/Constant.h>
#include <xzero-flow/vm/Signature.h>

namespace xzero::flow {

class IRBuiltinFunction : public Constant {
 public:
  IRBuiltinFunction(const IRBuiltinFunction&) = default;
  IRBuiltinFunction& operator=(const IRBuiltinFunction&) = default;

  IRBuiltinFunction(IRBuiltinFunction&&) = default;
  IRBuiltinFunction& operator=(IRBuiltinFunction&&) = default;

  IRBuiltinFunction(const Signature& sig, bool isSideEffectFree)
      : Constant(sig.returnType(), sig.name()), signature_(sig) {}

  const Signature& signature() const { return signature_; }
  const Signature& get() const { return signature_; }
  bool isSideEffectFree() const noexcept { return isSideEffectFree_; }

 private:
  Signature signature_;
  bool isSideEffectFree_;
};

inline bool operator==(const IRBuiltinFunction& f, const Signature& sig) {
  return sig == f.signature();
}

inline bool operator!=(const IRBuiltinFunction& f, const Signature& sig) {
  return sig != f.signature();
}

inline bool operator==(const Signature& sig, const IRBuiltinFunction& f) {
  return sig == f.signature();
}

inline bool operator!=(const Signature& sig, const IRBuiltinFunction& f) {
  return sig != f.signature();
}

}  // namespace xzero::flow
