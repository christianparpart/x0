#include <x0/flow/vm/Runtime.h>
#include <x0/flow/vm/NativeCallback.h>

namespace x0 {
namespace FlowVM {

Runtime::~Runtime()
{
    for (auto b: builtins_) {
        delete b;
    }
}

NativeCallback& Runtime::registerHandler(const std::string& name)
{
    printf("Runtime::registerHandler(): %s\n", name.c_str());
    builtins_.push_back(new NativeCallback(this, name));
    return *builtins_[builtins_.size() - 1];
}

NativeCallback& Runtime::registerFunction(const std::string& name, FlowType returnType)
{
    printf("Runtime::registerFunction(): %s\n", name.c_str());
    builtins_.push_back(new NativeCallback(this, name, returnType));
    return *builtins_[builtins_.size() - 1];
}

NativeCallback* Runtime::find(const std::string& signature)
{
    for (auto callback: builtins_) {
        if (callback && callback->signature().to_s() == signature) {
            return callback;
        }
    }

    return nullptr;
}

NativeCallback* Runtime::find(const Signature& signature)
{
    return find(signature.to_s());
}

void Runtime::unregisterNative(const std::string& name)
{
    printf("Runtime::unregisterNative(): %s\n", name.c_str());
    for (size_t i = 0, e = builtins_.size(); i != e; ++i) {
        if (builtins_[i] && builtins_[i]->signature().name() == name) {
            delete builtins_[i];
            builtins_[i] = nullptr;
            return;
        }
    }
}

} // namespace FlowVM
} // namespace x0
