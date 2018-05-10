// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/LiteralType.h>
#include <vector>
#include <string>

namespace xzero::flow {

class Signature {
 private:
  std::string name_;
  LiteralType returnType_;
  std::vector<LiteralType> args_;

 public:
  Signature();
  explicit Signature(const std::string& signature);
  Signature(Signature&&) = default;
  Signature(const Signature&) = default;
  Signature& operator=(Signature&&) = default;
  Signature& operator=(const Signature&) = default;

  void setName(const std::string& name) { name_ = name; }
  void setReturnType(LiteralType rt) { returnType_ = rt; }
  void setArgs(const std::vector<LiteralType>& args) { args_ = args; }
  void setArgs(std::vector<LiteralType>&& args) { args_ = std::move(args); }

  const std::string& name() const { return name_; }
  LiteralType returnType() const { return returnType_; }
  const std::vector<LiteralType>& args() const { return args_; }
  std::vector<LiteralType>& args() { return args_; }

  std::string to_s() const;

  bool operator==(const Signature& v) const { return to_s() == v.to_s(); }
  bool operator!=(const Signature& v) const { return to_s() != v.to_s(); }
  bool operator<(const Signature& v) const { return to_s() < v.to_s(); }
  bool operator>(const Signature& v) const { return to_s() > v.to_s(); }
  bool operator<=(const Signature& v) const { return to_s() <= v.to_s(); }
  bool operator>=(const Signature& v) const { return to_s() >= v.to_s(); }
};

LiteralType typeSignature(char ch);
char signatureType(LiteralType t);

}  // namespace xzero::flow

namespace fmt {
  template<>
  struct formatter<xzero::flow::Signature> {
    template <typename ParseContext>
    auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const xzero::flow::Signature& v, FormatContext &ctx) {
      return format_to(ctx.begin(), v.to_s());
    }
  };
}

