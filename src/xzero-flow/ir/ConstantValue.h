// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/ir/Constant.h>

#include <iostream>
#include <string>

namespace xzero {
  class Cidr;
  class IPAddress;
  class RegExp;
}

namespace xzero::flow {

template <typename T, const FlowType Ty>
class ConstantValue : public Constant {
 public:
  ConstantValue(const T& value, const std::string& name = "")
      : Constant(Ty, name), value_(value) {}

  T get() const { return value_; }

  void dump() override {
    std::cout << fmt::format("Constant '{}': {} = {}\n",
        name(), type(), value_);
  }

 private:
  T value_;
};

typedef ConstantValue<int64_t, FlowType::Number> ConstantInt;
typedef ConstantValue<bool, FlowType::Boolean> ConstantBoolean;
typedef ConstantValue<std::string, FlowType::String> ConstantString;
typedef ConstantValue<IPAddress, FlowType::IPAddress> ConstantIP;
typedef ConstantValue<Cidr, FlowType::Cidr> ConstantCidr;
typedef ConstantValue<RegExp, FlowType::RegExp> ConstantRegExp;

}  // namespace xzero::flow
