// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/ConstantArray.h>
#include <cstdlib>

namespace xzero::flow {

LiteralType ConstantArray::makeArrayType(LiteralType elementType) {
  switch (elementType) {
    case LiteralType::Number:
      return LiteralType::IntArray;
    case LiteralType::String:
      return LiteralType::StringArray;
    case LiteralType::IPAddress:
      return LiteralType::IPAddrArray;
    case LiteralType::Cidr:
      return LiteralType::CidrArray;
    case LiteralType::Boolean:
    case LiteralType::RegExp:
    case LiteralType::Handler:
    case LiteralType::IntArray:
    case LiteralType::StringArray:
    case LiteralType::IPAddrArray:
    case LiteralType::CidrArray:
    case LiteralType::Void:
      abort();
    default:
      abort();
  }
}

}  // namespace xzero::flow
