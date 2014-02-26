/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/Constant.h>

namespace x0 {

void Constant::dump()
{
    printf("Constant %zu '%s': %s\n", id_, name().c_str(), tos(type()).c_str());
}

} // namespace x0
