// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/ir/ConstantValue.h>
#include <xzero/RegExp.h>
#include <xzero/net/Cidr.h>
#include <xzero/net/IPAddress.h>

#include <string>

namespace xzero::flow {

template class XZERO_EXPORT ConstantValue<int64_t, LiteralType::Number>;
template class XZERO_EXPORT ConstantValue<bool, LiteralType::Boolean>;
template class XZERO_EXPORT ConstantValue<std::string, LiteralType::String>;
template class XZERO_EXPORT ConstantValue<IPAddress, LiteralType::IPAddress>;
template class XZERO_EXPORT ConstantValue<Cidr, LiteralType::Cidr>;
template class XZERO_EXPORT ConstantValue<RegExp, LiteralType::RegExp>;

}  // namespace xzero::flow
