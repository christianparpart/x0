// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/FlowType.h>
#include <vector>
#include <string>
#include <iosfwd>

namespace xzero::flow {

class Signature {
 private:
  std::string name_;
  FlowType returnType_;
  std::vector<FlowType> args_;

 public:
  Signature();
  explicit Signature(const std::string& signature);
  Signature(Signature&&) = default;
  Signature(const Signature&) = default;
  Signature& operator=(Signature&&) = default;
  Signature& operator=(const Signature&) = default;

  void setName(const std::string& name) { name_ = name; }
  void setReturnType(FlowType rt) { returnType_ = rt; }
  void setArgs(const std::vector<FlowType>& args) { args_ = args; }
  void setArgs(std::vector<FlowType>&& args) { args_ = std::move(args); }

  const std::string& name() const { return name_; }
  FlowType returnType() const { return returnType_; }
  const std::vector<FlowType>& args() const { return args_; }
  std::vector<FlowType>& args() { return args_; }

  std::string to_s() const;

  bool operator==(const Signature& v) const { return to_s() == v.to_s(); }
  bool operator!=(const Signature& v) const { return to_s() != v.to_s(); }
  bool operator<(const Signature& v) const { return to_s() < v.to_s(); }
  bool operator>(const Signature& v) const { return to_s() > v.to_s(); }
  bool operator<=(const Signature& v) const { return to_s() <= v.to_s(); }
  bool operator>=(const Signature& v) const { return to_s() >= v.to_s(); }
};

FlowType typeSignature(char ch);
char signatureType(FlowType t);

}  // namespace xzero::flow

namespace fmt {
  template<>
  struct formatter<xzero::flow::Signature> {
    using Signature = xzero::flow::Signature;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const Signature& v, FormatContext &ctx) {
      return format_to(ctx.begin(), v.to_s());
    }
  };
}

