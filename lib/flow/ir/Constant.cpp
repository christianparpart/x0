// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/flow/ir/Constant.h>

namespace x0 {

void Constant::dump()
{
    printf("Constant '%s': %s\n", name().c_str(), tos(type()).c_str());
}

} // namespace x0
