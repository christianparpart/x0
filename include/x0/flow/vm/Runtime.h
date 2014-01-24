#pragma once

#include <x0/Api.h>
#include <x0/flow/FlowType.h>
#include <x0/flow/vm/Params.h>
#include <x0/flow/vm/Signature.h>
#include <string>
#include <vector>
#include <functional>

namespace x0 {

class Unit;

namespace FlowVM {

typedef uint64_t Value;

class Runner;
class NativeCallback;

class X0_API Runtime
{
public:
    virtual ~Runtime();

    virtual bool import(const std::string& name, const std::string& path, std::vector<FlowVM::NativeCallback*>* builtins) = 0;

    bool contains(const std::string& signature) const;
    NativeCallback* find(const std::string& signature);
    NativeCallback* find(const Signature& signature);
    const std::vector<NativeCallback*>& builtins() const { return builtins_; }

    NativeCallback& registerHandler(const std::string& name);
    NativeCallback& registerFunction(const std::string& name, FlowType returnType);
    void unregisterNative(const std::string& name);

    void invoke(int id, int argc, Value* argv, Runner* cx);

    bool verify(Unit* unit);

private:
    std::vector<NativeCallback*> builtins_;
};

} // FlowVM
} // x0
