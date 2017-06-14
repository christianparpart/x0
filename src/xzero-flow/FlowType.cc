// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/FlowType.h>
#include <xzero/RuntimeError.h>

namespace xzero {
namespace flow {

std::string tos(FlowType type) {
  switch (type) {
    case FlowType::Void:
      return "void";
    case FlowType::Boolean:
      return "bool";
    case FlowType::Number:
      return "int";
    case FlowType::String:
      return "string";
    case FlowType::IPAddress:
      return "IPAddress";
    case FlowType::Cidr:
      return "Cidr";
    case FlowType::RegExp:
      return "RegExp";
    case FlowType::Handler:
      return "HandlerRef";
    case FlowType::IntArray:
      return "IntArray";
    case FlowType::StringArray:
      return "StringArray";
    case FlowType::IPAddrArray:
      return "IPAddrArray";
    case FlowType::CidrArray:
      return "CidrArray";
    default:
      RAISE_STATUS(InvalidArgumentError);
  }
}

bool isArrayType(FlowType type) {
  switch (type) {
    case FlowType::IntArray:
    case FlowType::StringArray:
    case FlowType::IPAddrArray:
    case FlowType::CidrArray:
      return true;
    default:
      return false;
  }
}

FlowType elementTypeOf(FlowType type) {
  switch (type) {
    case FlowType::Void:
    case FlowType::Boolean:
    case FlowType::Number:
    case FlowType::String:
    case FlowType::IPAddress:
    case FlowType::Cidr:
    case FlowType::RegExp:
    case FlowType::Handler:
      return type;
    case FlowType::IntArray:
      return FlowType::Number;
    case FlowType::StringArray:
      return FlowType::String;
    case FlowType::IPAddrArray:
      return FlowType::IPAddress;
    case FlowType::CidrArray:
      return FlowType::Cidr;
    default:
      RAISE_STATUS(InvalidArgumentError);
  }
}

}  // namespace flow
}  // namespace xzero
