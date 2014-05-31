/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/ConstantArray.h>
#include <cstdlib>

namespace x0 {

FlowType ConstantArray::makeArrayType(FlowType elementType)
{
    switch (elementType) {
        case FlowType::Number:
            return FlowType::IntArray;
        case FlowType::String:
            return FlowType::StringArray;
        case FlowType::IPAddress:
            return FlowType::IPAddrArray;
        case FlowType::Cidr:
            return FlowType::CidrArray;
        case FlowType::Boolean:
        case FlowType::RegExp:
        case FlowType::Handler:
        case FlowType::IntArray:
        case FlowType::StringArray:
        case FlowType::IPAddrArray:
        case FlowType::CidrArray:
        case FlowType::Void:
            abort();
        default:
            abort();
    }
}

} // namespace x0
