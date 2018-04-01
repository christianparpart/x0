// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>
#include <fmt/format.h>
#include <string>
#include <cstdint>
#include <vector>

namespace xzero {

class Cidr;

namespace flow {

//! \addtogroup Flow
//@{

enum class FlowType {
  Void = 0,
  Boolean = 1,       // bool (int64)
  Number = 2,        // int64
  String = 3,        // BufferRef*
  IPAddress = 5,     // IPAddress*
  Cidr = 6,          // Cidr*
  RegExp = 7,        // RegExp*
  Handler = 8,       // bool (*native_handler)(FlowContext*);
  IntArray = 9,      // array<int>
  StringArray = 10,  // array<string>
  IPAddrArray = 11,  // array<IPAddress>
  CidrArray = 12,    // array<Cidr>
};

typedef uint64_t Register;  // vm

typedef int64_t FlowNumber;
typedef std::string FlowString;

std::string tos(FlowType type);

bool isArrayType(FlowType type);
FlowType elementTypeOf(FlowType type);

// {{{ array types
class FlowArray {
 public:
  size_t size() const { return base_[0]; }

 protected:
  explicit FlowArray(const Register* base) : base_(base) {}

  Register getRawAt(size_t i) const { return base_[1 + i]; }
  const Register* data() const { return base_ + 1; }

 protected:
  const Register* base_;
};

typedef std::vector<FlowNumber> FlowIntArray;
typedef std::vector<FlowString> FlowStringArray;
typedef std::vector<IPAddress> FlowIPAddrArray;
typedef std::vector<Cidr> FlowCidrArray;

// }}}

//!@}

}  // namespace flow
}  // namespace xzero

namespace std {
  template <>
  struct hash<xzero::flow::FlowType> {
    uint32_t operator()(xzero::flow::FlowType v) const noexcept {
      return static_cast<uint32_t>(v);
    }
  };
}

namespace fmt {
  template<>
  struct formatter<xzero::flow::FlowType> {
    using FlowType = xzero::flow::FlowType;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const FlowType& v, FormatContext &ctx) {
      switch (v) {
        case FlowType::Void: return format_to(ctx.begin(), "Void");
        case FlowType::Boolean: return format_to(ctx.begin(), "Boolean");
        case FlowType::Number: return format_to(ctx.begin(), "Number");
        case FlowType::String: return format_to(ctx.begin(), "String");
        case FlowType::IPAddress: return format_to(ctx.begin(), "IPAddress");
        case FlowType::Cidr: return format_to(ctx.begin(), "Cidr");
        case FlowType::RegExp: return format_to(ctx.begin(), "RegExp");
        case FlowType::Handler: return format_to(ctx.begin(), "Handler");
        case FlowType::IntArray: return format_to(ctx.begin(), "IntArray");
        case FlowType::StringArray: return format_to(ctx.begin(), "StringArray");
        case FlowType::IPAddrArray: return format_to(ctx.begin(), "IPAddrArray");
        case FlowType::CidrArray: return format_to(ctx.begin(), "CidrArray");
        default:
          return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}

