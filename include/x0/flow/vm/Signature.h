#pragma once

#include <x0/flow/FlowType.h>
#include <vector>
#include <string>

namespace x0 {
namespace FlowVM {

class X0_API Signature {
private:
    std::string name_;
    FlowType returnType_;
    std::vector<FlowType> args_;

public:
    Signature();
    Signature(const std::string& signature);
    Signature(const Signature& v) :
        name_(v.name_),
        returnType_(v.returnType_),
        args_(v.args_)
    {}

    void setName(const std::string& name) { name_ = name; }
    void setReturnType(FlowType rt) { returnType_ = rt; }
    void setArgs(const std::vector<FlowType>& args) { args_ = args; }
    void setArgs(std::vector<FlowType>&& args) { args_ = std::move(args); }

    const std::string& name() const { return name_; }
    FlowType returnType() const { return returnType_; }
    const std::vector<FlowType>& args() const { return args_; }
    std::vector<FlowType>& args() { return args_; }

    std::string to_s() const;

    bool operator==(const Signature& v) const { return to_s() == v.to_s(); }
    bool operator!=(const Signature& v) const { return to_s() != v.to_s(); }
    bool operator<(const Signature& v) const { return to_s() < v.to_s(); }
    bool operator>(const Signature& v) const { return to_s() > v.to_s(); }
    bool operator<=(const Signature& v) const { return to_s() <= v.to_s(); }
    bool operator>=(const Signature& v) const { return to_s() >= v.to_s(); }
};

FlowType typeSignature(char ch);
char signatureType(FlowType t);

} // namespace FlowVM
} // namespace x0
