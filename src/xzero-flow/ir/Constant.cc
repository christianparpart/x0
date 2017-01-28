// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/ir/Constant.h>

namespace xzero {
namespace flow {

void Constant::dump() {
  printf("Constant '%s': %s\n", name().c_str(), tos(type()).c_str());
}

}  // namespace flow
}  // namespace xzero
