// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/ir/Constant.h>
#include <xzero-flow/Signature.h>
#include <xzero-flow/NativeCallback.h>

namespace xzero::flow {

class IRBuiltinFunction : public Constant {
 public:
  explicit IRBuiltinFunction(const NativeCallback& cb)
      : Constant(cb.signature().returnType(), cb.signature().name()),
        native_(cb) {}

  const Signature& signature() const { return native_.signature(); }
  const NativeCallback& getNative() const { return native_; }

 private:
  const NativeCallback& native_;
};

}  // namespace xzero::flow
