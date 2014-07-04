// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/Api.h>
#include <string>

namespace x0 {
namespace FlowVM {

enum class MatchClass { Same, Head, Tail, RegExp, };

X0_API std::string tos(MatchClass c);

}  // namespace FlowVM
}  // namespace x0
