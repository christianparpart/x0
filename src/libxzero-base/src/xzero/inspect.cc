// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include <xzero/inspect.h>

namespace xzero {

template <>
std::string inspect<bool>(const bool& value) {
  return value == true ? "true" : "false";
}

template <>
std::string inspect<int>(const int& value) {
  return std::to_string(value);
}

template <>
std::string inspect<unsigned int>(const unsigned int& value) {
  return std::to_string(value);
}

template <>
std::string inspect<unsigned long>(const unsigned long& value) {
  return std::to_string(value);
}

template <>
std::string inspect<unsigned long long>(const unsigned long long& value) {
  return std::to_string(value);
}

template <>
std::string inspect<unsigned char>(const unsigned char& value) {
  return std::to_string(value);
}

template <>
std::string inspect<long long>(const long long& value) {
  return std::to_string(value);
}

template <>
std::string inspect<long>(const long& value) {
  return std::to_string(value);
}

template <>
std::string inspect<unsigned short>(const unsigned short& value) {
  return std::to_string(value);
}

template <>
std::string inspect<float>(const float& value) {
  return std::to_string(value);
}

template <>
std::string inspect<double>(const double& value) {
  return std::to_string(value);
}

template <>
std::string inspect<std::string>(const std::string& value) {
  return value;
}

template <>
std::string inspect<std::wstring>(const std::wstring& value) {
  std::string out;
  out.assign(value.begin(), value.end());
  return out;
}

template <>
std::string inspect<char const*>(char const* const& value) {
  return std::string(value);
}

template <>
std::string inspect<void*>(void* const& value) {
  return "<ptr>";
}

template <>
std::string inspect<const void*>(void const* const& value) {
  return "<ptr>";
}

template <>
std::string inspect<std::exception>(const std::exception& e) {
  return e.what();
}

} // namespace xzero
