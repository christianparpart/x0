/* <x0/flow/ir/Value.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/FlowType.h>

#include <string>
#include <vector>

namespace x0 {

class Instr;

/**
 * Defines an immutable IR value.
 */
class X0_API Value {
protected:
    Value(const Value& v);

public:
    Value(FlowType ty, const std::string& name);
    virtual ~Value();

    FlowType type() const { return type_; }
    void setType(FlowType ty) { type_ = ty; }

    const std::string& name() const { return name_; }
    void setName(const std::string& n) { name_ = n; }

    void addUse(Instr* user);
    void removeUse(Instr* user);
    bool isUsed() const { return !uses_.empty(); }
    const std::vector<Instr*>& uses() const { return uses_; }

    /**
     * @brief Replaces all uses of \c this value as operand with value \p newUse instead.
     * @param newUse the new value to be used.
     */
    void replaceAllUsesWith(Value* newUse);

    virtual void dump() = 0;

private:
    FlowType type_;
    std::string name_;

    std::vector<Instr*> uses_; //! list of instructions that <b>use</b> this value.
};

} // namespace x0
