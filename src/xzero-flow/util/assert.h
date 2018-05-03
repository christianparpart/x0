// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <cassert>
#include <cstdlib>
#include <string>

#define FLOW_ASSERT(cond, msg) if (!(cond)) {             \
  fprintf(stderr, "%s\n", std::string(msg).c_str());      \
  abort();                                                \
}
