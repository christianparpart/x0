// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/Constant.h>

namespace xzero::flow {

void Constant::dump() {
  printf("Constant '%s': %s\n", name().c_str(), tos(type()).c_str());
}

}  // namespace xzero::flow
