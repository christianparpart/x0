// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/ConstantValue.h>
#include <flow/util/RegExp.h>
#include <xzero/net/Cidr.h>
#include <xzero/net/IPAddress.h>

#include <string>

namespace xzero::flow {

template class ConstantValue<int64_t, LiteralType::Number>;
template class ConstantValue<bool, LiteralType::Boolean>;
template class ConstantValue<std::string, LiteralType::String>;
template class ConstantValue<IPAddress, LiteralType::IPAddress>;
template class ConstantValue<Cidr, LiteralType::Cidr>;
template class ConstantValue<util::RegExp, LiteralType::RegExp>;

}  // namespace xzero::flow
