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

#include <string>
#include <vector>
#include <list>

namespace xzero::flow {

class IRBuiltinHandler : public Constant {
 public:
  IRBuiltinHandler(const IRBuiltinHandler&) = default;
  IRBuiltinHandler& operator=(const IRBuiltinHandler&) = default;

  IRBuiltinHandler(IRBuiltinHandler&&) = default;
  IRBuiltinHandler& operator=(IRBuiltinHandler&&) = default;

  IRBuiltinHandler(const Signature& sig, bool isNeverReturning)
      : Constant(FlowType::Boolean, sig.name()),
        signature_(sig),
        isNeverReturning_(isNeverReturning) {}

  const Signature& signature() const { return signature_; }
  const Signature& get() const { return signature_; }
  bool isNeverReturning() const { return isNeverReturning_; }

  bool operator==(const Signature& sig) const { return sig == signature_; }
  bool operator!=(const Signature& sig) const { return sig != signature_; }

 private:
  Signature signature_;
  bool isNeverReturning_;
};

inline bool operator==(const Signature& a, const IRBuiltinHandler& b) {
  return b == a;
}

inline bool operator!=(const Signature& a, const IRBuiltinHandler& b) {
  return b == a;
}

}  // namespace xzero::flow
