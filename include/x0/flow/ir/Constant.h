/* <x0/flow/ir/xxx.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Value.h>

namespace x0 {

class X0_API Constant : public Value {
public:
    Constant(FlowType ty, const std::string& name) :
        Value(ty, name)  {}

    void dump() override;
};

} // namespace x0
