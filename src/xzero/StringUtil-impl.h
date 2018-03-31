// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <sstream>

namespace xzero {

template<typename T>
inline std::string to_string(const T& value) {
  std::stringstream sstr;
  sstr << value;
  return sstr.str();
}

template<typename T>
inline std::string to_string(T&& value) {
  std::stringstream sstr;
  sstr << value;
  return sstr.str();
}

} // namespace xzero
