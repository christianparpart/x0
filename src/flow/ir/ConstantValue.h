// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/ir/Constant.h>

#include <iostream>
#include <string>

namespace xzero {
  class Cidr;
  class IPAddress;
}

namespace xzero::flow::util {
  class RegExp;
}

namespace xzero::flow {

template <typename T, const LiteralType Ty>
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

typedef ConstantValue<int64_t, LiteralType::Number> ConstantInt;
typedef ConstantValue<bool, LiteralType::Boolean> ConstantBoolean;
typedef ConstantValue<std::string, LiteralType::String> ConstantString;
typedef ConstantValue<IPAddress, LiteralType::IPAddress> ConstantIP;
typedef ConstantValue<Cidr, LiteralType::Cidr> ConstantCidr;
typedef ConstantValue<util::RegExp, LiteralType::RegExp> ConstantRegExp;

}  // namespace xzero::flow
