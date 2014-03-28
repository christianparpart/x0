/* <x0/flow/ir/ConstantArray.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Constant.h>

#include <string>
#include <vector>

namespace x0 {

class X0_API ConstantArray : public Constant {
public:
    ConstantArray(size_t id, const std::vector<Constant*>& elements, const std::string& name = "") :
        Constant(makeArrayType(elements.front()->type()), id, name),
        elements_(elements)
    {}

    const std::vector<Constant*>& get() const { return elements_; }

    FlowType elementType() const { return elements_[0]->type(); }

private:
    std::vector<Constant*> elements_;

    FlowType makeArrayType(FlowType elementType);
};

} // namespace x0
