#include <x0/flow/vm/Runtime.h>
#include <x0/flow/vm/NativeCallback.h>

namespace x0 {
namespace FlowVM {

NativeCallback& Runtime::registerHandler(const std::string& name)
{
    builtins_.push_back(new NativeCallback(this, name));
    return *builtins_[builtins_.size() - 1];
}

NativeCallback& Runtime::registerFunction(const std::string& name, FlowType returnType)
{
    builtins_.push_back(new NativeCallback(this, name, returnType));
    return *builtins_[builtins_.size() - 1];
}

NativeCallback* Runtime::find(const std::string& signature)
{
    for (auto callback: builtins_)
        if (callback->signature().to_s() == signature)
            return callback;

    return nullptr;
}

void Runtime::unregisterNative(const std::string& name)
{
    for (auto i = builtins_.begin(), e = builtins_.end(); i != e; ++i) {
        if ((*i)->signature().name() == name) {
            builtins_.erase(i);
            return;
        }
    }
}

} // namespace FlowVM
} // namespace x0
