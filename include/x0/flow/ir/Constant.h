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
    Constant(FlowType ty, size_t id, const std::string& name) :
        Value(ty, name), id_(id) {}

    size_t id() const { return id_; }

    void dump() override;

private:
    int64_t id_;
};

} // namespace x0
