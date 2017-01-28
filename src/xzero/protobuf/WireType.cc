// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/protobuf/WireType.h>
#include <xzero/StringUtil.h>

namespace xzero {

template<>
std::string StringUtil::toString<protobuf::WireType>(protobuf::WireType wt) {
  switch (wt) {
    case protobuf::WireType::Varint:
      return "Varint";
    case protobuf::WireType::Fixed64:
      return "Fixed64";
    case protobuf::WireType::LengthDelimited:
      return "LengthDelimited";
    case protobuf::WireType::Fixed32:
      return "Fixed32";
    default:
      return StringUtil::format("<$0>", (unsigned) wt);
  }
}

} // namespace xzero
