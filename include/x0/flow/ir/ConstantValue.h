/* <x0/flow/ir/ConstantValue.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Constant.h>
#include <x0/IPAddress.h>
#include <x0/RegExp.h>
#include <x0/Cidr.h>

#include <string>

namespace x0 {

template<typename T, const FlowType Ty>
class X0_API ConstantValue : public Constant {
public:
    ConstantValue(size_t id, const T& value, const std::string& name = "") :
        Constant(Ty, id, name),
        value_(value)
        {}

    T get() const { return value_; }

private:
    T value_;
};

typedef ConstantValue<int64_t, FlowType::Number> ConstantInt;
typedef ConstantValue<bool, FlowType::Boolean> ConstantBoolean;
typedef ConstantValue<std::string, FlowType::String> ConstantString;
typedef ConstantValue<IPAddress, FlowType::IPAddress> ConstantIP;
typedef ConstantValue<Cidr, FlowType::Cidr> ConstantCidr;
typedef ConstantValue<RegExp, FlowType::RegExp> ConstantRegExp;

} // namespace x0
