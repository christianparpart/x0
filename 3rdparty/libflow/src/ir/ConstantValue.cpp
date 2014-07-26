// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/ConstantValue.h>

namespace flow {

template class BASE_EXPORT ConstantValue<int64_t, FlowType::Number>;
template class BASE_EXPORT ConstantValue<bool, FlowType::Boolean>;
template class BASE_EXPORT ConstantValue<std::string, FlowType::String>;
template class BASE_EXPORT ConstantValue<IPAddress, FlowType::IPAddress>;
template class BASE_EXPORT ConstantValue<Cidr, FlowType::Cidr>;
template class BASE_EXPORT ConstantValue<RegExp, FlowType::RegExp>;

}  // namespace flow
