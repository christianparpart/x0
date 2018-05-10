// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/LiteralType.h>
#include <flow/util/assert.h>

namespace xzero {
namespace flow {

std::string tos(LiteralType type) {
  switch (type) {
    case LiteralType::Void:
      return "void";
    case LiteralType::Boolean:
      return "bool";
    case LiteralType::Number:
      return "int";
    case LiteralType::String:
      return "string";
    case LiteralType::IPAddress:
      return "IPAddress";
    case LiteralType::Cidr:
      return "Cidr";
    case LiteralType::RegExp:
      return "RegExp";
    case LiteralType::Handler:
      return "HandlerRef";
    case LiteralType::IntArray:
      return "IntArray";
    case LiteralType::StringArray:
      return "StringArray";
    case LiteralType::IPAddrArray:
      return "IPAddrArray";
    case LiteralType::CidrArray:
      return "CidrArray";
    default:
      FLOW_ASSERT(false, "InvalidArgumentError");
  }
}

bool isArrayType(LiteralType type) {
  switch (type) {
    case LiteralType::IntArray:
    case LiteralType::StringArray:
    case LiteralType::IPAddrArray:
    case LiteralType::CidrArray:
      return true;
    default:
      return false;
  }
}

LiteralType elementTypeOf(LiteralType type) {
  switch (type) {
    case LiteralType::Void:
    case LiteralType::Boolean:
    case LiteralType::Number:
    case LiteralType::String:
    case LiteralType::IPAddress:
    case LiteralType::Cidr:
    case LiteralType::RegExp:
    case LiteralType::Handler:
      return type;
    case LiteralType::IntArray:
      return LiteralType::Number;
    case LiteralType::StringArray:
      return LiteralType::String;
    case LiteralType::IPAddrArray:
      return LiteralType::IPAddress;
    case LiteralType::CidrArray:
      return LiteralType::Cidr;
    default:
      FLOW_ASSERT(false, "InvalidArgumentError");
  }
}

}  // namespace flow
}  // namespace xzero
