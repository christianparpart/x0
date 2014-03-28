/* <x0/flow/ir/xxx.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Constant.h>
#include <x0/flow/vm/Signature.h>

#include <string>
#include <vector>
#include <list>

namespace x0 {

class X0_API IRBuiltinHandler : public Constant {
public:
    IRBuiltinHandler(const FlowVM::Signature& sig) :
        Constant(FlowType::Boolean, sig.name()),
        signature_(sig)
    {}

    const FlowVM::Signature& signature() const { return signature_; }
    const FlowVM::Signature& get() const { return signature_; }

private:
    FlowVM::Signature signature_;
};

} // namespace x0
