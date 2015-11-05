// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/StringUtil.h>
#include <xzero/Buffer.h>
#include <cstdio>

namespace xzero {

template <typename T1, typename T2>
std::string inspect(const std::pair<T1, T2>& value) {
  std::string str = "<";
  str += inspect(value.first);
  str += ", ";
  str += inspect(value.second);
  str += '>';
  return str;
}

template <typename T>
std::string inspect(const std::vector<T>& value) {
  std::string str = "[";
  for (auto iter = value.begin(); iter != value.end(); ++iter) {
    if (iter != value.begin()) {
      str += ", ";
    }

    str += inspect(*iter);
  }
  str += ']';
  return str;
}

template <typename T>
std::string inspect(const std::set<T>& value) {
  std::string str = "{";
  for (auto iter = value.begin(); iter != value.end(); ++iter) {
    if (iter != value.begin()) {
      str += ", ";
    }

    str += inspect(*iter);
  }
  str += '}';
  return str;
}

template <typename T>
std::string inspect(T* value) {
  return "@@x" + BufferRef::hexdump(&value, sizeof(void*), HexDumpMode::InlineNarrow);
}

template <typename H, typename... T>
std::vector<std::string> inspectAll(H head, T... tail) {
  auto vec = inspectAll(tail...);
  vec.insert(vec.begin(), inspect(head));
  return vec;
}

template <typename H>
std::vector<std::string> inspectAll(H head) {
  std::vector<std::string> vec;
  vec.push_back(inspect(head));
  return vec;
}

// TODO: move into its own more correct file
template <typename... T>
void iputs(const char* fmt, T... values) {
  auto str = StringUtil::formatv(fmt, inspectAll(values...));
  printf("%*s\n", str.size(), str.data());
}

} // namespace xzero


