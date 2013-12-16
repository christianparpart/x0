#pragma once

#include <x0/flow2/FlowType.h>
#include <x0/flow2/vm/Signature.h>
#include <string>
#include <vector>
#include <functional>

namespace x0 {
namespace FlowVM {

typedef uint64_t Value;

class Params;
class Runner;
class Runtime;

class NativeCallback {
public:
    typedef std::function<void(Params& args)> Functor;

private:
    Runtime* runtime_;
    bool isHandler_;
    Functor function_;
    Signature signature_;

public:
    // constructs a handler callback
    NativeCallback(Runtime* runtime, const std::string& _name) :
        runtime_(runtime),
        isHandler_(true),
        function_(),
        signature_()
    {
        signature_.setName(_name);
        signature_.setReturnType(FlowType::Boolean);
    }

    // constructs a function callback
    NativeCallback(Runtime* runtime, const std::string& _name, FlowType _returnType) :
        runtime_(runtime),
        isHandler_(false),
        function_(),
        signature_()
    {
        signature_.setName(_name);
        signature_.setReturnType(_returnType);
    }

    NativeCallback(const std::string& _name, const Functor& _builtin, FlowType _returnType) :
        runtime_(nullptr),
        isHandler_(false),
        function_(_builtin),
        signature_()
    {
        signature_.setName(_name);
        signature_.setReturnType(_returnType);
    }

    bool isHandler() const { return isHandler_; }
    const std::string name() const { return signature_.name(); }
    const Signature& signature() const { return signature_; }

    void invoke(Params& args) const {
        function_(args);
    }

    template<typename Arg1, typename... Args>
    NativeCallback& signature(Arg1 a1, Args... more) {
        signature_.setArgs({a1, more...});
        return *this;
    }

    NativeCallback& operator()(const Functor& cb) {
        function_ = cb;
        return *this;
    }

    NativeCallback& bind(const Functor& cb) {
        function_ = cb;
        return *this;
    }

    template<typename Class>
    NativeCallback& bind(void (Class::*method)(Params&), Class* obj) {
        function_ = std::bind(method, obj, std::placeholders::_1);
        return *this;
    }

    template<typename Class>
    NativeCallback& bind(void (Class::*method)(Params&)) {
        function_ = std::bind(method, static_cast<Class*>(runtime_), std::placeholders::_1);
        return *this;
    }
};

} // namespace FlowVM
} // namespace x0
