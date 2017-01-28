// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HeaderField.h>
#include <xzero/StringUtil.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {

HeaderField HeaderField::parse(const std::string& field) {
  size_t i = field.find(':');
  size_t k = i + 1;

  while (std::isspace(field[k]))
    k++;

  std::string name = field.substr(0, i);
  std::string value = field.substr(k);

  return HeaderField(name, value);
}

bool HeaderField::operator==(const HeaderField& other) const {
  return iequals(name(), other.name()) && iequals(value(), other.value());
}

bool HeaderField::operator!=(const HeaderField& other) const {
  return !(*this == other);
}

std::string inspect(const HeaderField& field) {
  return StringUtil::format("HeaderField(\"$0\", \"$1\")",
                            field.name(),
                            field.value());
}

} // namespace http

template <>
std::string StringUtil::toString(http::HeaderField field) {
  return StringUtil::format("{\"$0\": \"$1\"}", field.name(), field.value());
}

} // namespace xzero
