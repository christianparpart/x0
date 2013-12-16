#pragma once

#include <x0/Api.h>
#include <x0/flow2/FlowType.h>
#include <x0/flow2/vm/Params.h>
#include <x0/flow2/vm/Signature.h>
#include <string>
#include <vector>
#include <functional>

namespace x0 {
namespace FlowVM {

typedef uint64_t Value;

class Runner;
class NativeCallback;

class X0_API Runtime
{
public:
    virtual bool import(const std::string& name, const std::string& path) = 0;

    bool contains(const std::string& signature) const;
    NativeCallback* find(const std::string& signature);
    const std::vector<NativeCallback>& builtins() const { return builtins_; }

    NativeCallback& registerHandler(const std::string& name);
    NativeCallback& registerFunction(const std::string& name, FlowType returnType);

    void invoke(int id, int argc, Value* argv, Runner* cx);

private:
    std::vector<NativeCallback> builtins_;
};

} // FlowVM
} // x0
