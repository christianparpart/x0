/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/Value.h>
#include <assert.h>

namespace x0 {

Value::Value(FlowType ty, const std::string& name) :
    type_(ty),
    name_(name),
    uses_()
{
    if (name_.empty()) {
        static unsigned long long i = 1;
        char buf[256];
        snprintf(buf, sizeof(buf), "unnamed%llu", i);
        i++;
        name_ = buf;
        //printf("default-create name: %s\n", name_.c_str());
    }
}

Value::~Value()
{
}

void Value::addUse(Instr* user)
{
    uses_.push_back(user);
}

void Value::dump()
{
    printf("Value '%s': %s\n", name_.c_str(), tos(type_).c_str());
}

} // namespace x0
