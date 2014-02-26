/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/ConstantValue.h>

namespace x0 {

template class X0_EXPORT ConstantValue<int64_t, FlowType::Number>;
template class X0_EXPORT ConstantValue<bool, FlowType::Boolean>;
template class X0_EXPORT ConstantValue<std::string, FlowType::String>;
template class X0_EXPORT ConstantValue<IPAddress, FlowType::IPAddress>;
template class X0_EXPORT ConstantValue<Cidr, FlowType::Cidr>;
template class X0_EXPORT ConstantValue<RegExp, FlowType::RegExp>;

} // namespace x0
