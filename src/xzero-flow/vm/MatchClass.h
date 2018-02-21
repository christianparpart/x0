// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <string>

namespace xzero {
namespace flow {
namespace vm {

enum class MatchClass { Same, Head, Tail, RegExp, };

std::string tos(MatchClass c);

}  // namespace vm
}  // namespace flow
}  // namespace xzero
