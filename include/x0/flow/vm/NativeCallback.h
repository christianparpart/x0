#pragma once

#include <x0/Api.h>
#include <x0/flow/FlowType.h>
#include <x0/flow/vm/Signature.h>
#include <x0/IPAddress.h>
#include <x0/Cidr.h>
#include <x0/RegExp.h>
#include <string>
#include <vector>
#include <functional>

namespace x0 {
namespace FlowVM {

typedef uint64_t Value;

class Params;
class Runner;
class Runtime;

class X0_API NativeCallback {
public:
    typedef std::function<void(Params& args)> Functor;

private:
    Runtime* runtime_;
    bool isHandler_;
    Functor function_;
    Signature signature_;

    // following attribs are irrelevant to the VM but useful for the frontend
    std::vector<std::string> names_;
    std::vector<void*> defaults_;

public:
    NativeCallback(Runtime* runtime, const std::string& _name);
    NativeCallback(Runtime* runtime, const std::string& _name, FlowType _returnType);
    ~NativeCallback();

    bool isHandler() const;
    const std::string name() const;
    const Signature& signature() const;

    // signature builder
    NativeCallback& returnType(FlowType type);

    template<typename T> NativeCallback& param(const std::string& name); //!< Declare a single named parameter.
    template<typename T> NativeCallback& param(const std::string& name, T defaultValue); //!< Declare a singly named parameter with default value.
    template<typename... Args> NativeCallback& params(Args... args); //!< Declare ordered parameter signature.

    NativeCallback& bind(const Functor& cb);
    template<typename Class> NativeCallback& bind(void (Class::*method)(Params&), Class* obj);
    template<typename Class> NativeCallback& bind(void (Class::*method)(Params&));

    // named parameter handling
    bool isNamed() const;
    const std::string& getNameAt(size_t i) const;
    const void* getDefaultAt(size_t i) const;
    int find(const std::string& name) const;

    // runtime
    void invoke(Params& args) const;
};

// {{{ inlines
inline NativeCallback& NativeCallback::returnType(FlowType type)
{
    signature_.setReturnType(type);
    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<bool>(const std::string& name)
{
    signature_.args().push_back(FlowType::Boolean);
    names_.push_back(name);
    defaults_.push_back(nullptr /*no default value*/);

    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<bool>(const std::string& name, bool defaultValue)
{
    signature_.args().push_back(FlowType::Boolean);
    names_.push_back(name);

    bool* value = new bool;
    *value = defaultValue;

    defaults_.push_back((void*) value);
    
    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<FlowNumber>(const std::string& name)
{
    signature_.args().push_back(FlowType::Number);
    names_.push_back(name);

    defaults_.push_back(nullptr /*no default value*/);

    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<FlowNumber>(const std::string& name, FlowNumber defaultValue)
{
    signature_.args().push_back(FlowType::Number);
    names_.push_back(name);

    FlowNumber* value = new FlowNumber;
    *value = defaultValue;

    defaults_.push_back((void*) value);
    
    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<int>(const std::string& name)
{
    signature_.args().push_back(FlowType::Number);
    names_.push_back(name);
    defaults_.push_back(nullptr /*no default value*/);

    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<int>(const std::string& name, int defaultValue)
{
    signature_.args().push_back(FlowType::Number);
    names_.push_back(name);
    defaults_.push_back((void*) (long long) defaultValue);
    
    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<FlowString>(const std::string& name)
{
    signature_.args().push_back(FlowType::String);
    names_.push_back(name);
    defaults_.push_back(nullptr /*no default value*/);

    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<FlowString>(const std::string& name, FlowString defaultValue)
{
    signature_.args().push_back(FlowType::String);
    names_.push_back(name);
    defaults_.push_back((void*) new FlowString(defaultValue));
    
    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<IPAddress>(const std::string& name)
{
    signature_.args().push_back(FlowType::IPAddress);
    names_.push_back(name);
    defaults_.push_back(nullptr /*no default value*/);

    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<IPAddress>(const std::string& name, IPAddress defaultValue)
{
    signature_.args().push_back(FlowType::IPAddress);
    names_.push_back(name);
    defaults_.push_back((void*) new IPAddress(defaultValue));
    
    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<Cidr>(const std::string& name)
{
    signature_.args().push_back(FlowType::Cidr);
    names_.push_back(name);
    defaults_.push_back(nullptr /*no default value*/);

    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<Cidr>(const std::string& name, Cidr defaultValue)
{
    signature_.args().push_back(FlowType::Cidr);
    names_.push_back(name);
    defaults_.push_back((void*) new Cidr(defaultValue));
    
    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<RegExp>(const std::string& name)
{
    signature_.args().push_back(FlowType::RegExp);
    names_.push_back(name);
    defaults_.push_back(nullptr /*no default value*/);

    return *this;
}

template<>
inline NativeCallback& NativeCallback::param<RegExp>(const std::string& name, RegExp defaultValue)
{
    signature_.args().push_back(FlowType::RegExp);
    names_.push_back(name);
    defaults_.push_back((void*) new RegExp(defaultValue));
    
    return *this;
}

// ------------------------------------------------------------------------------------------

template<typename... Args>
inline NativeCallback& NativeCallback::params(Args... args)
{
    signature_.setArgs({args...});
    return *this;
}

inline NativeCallback& NativeCallback::bind(const Functor& cb)
{
    function_ = cb;
    return *this;
}

template<typename Class>
inline NativeCallback& NativeCallback::bind(void (Class::*method)(Params&), Class* obj)
{
    function_ = std::bind(method, obj, std::placeholders::_1);
    return *this;
}

template<typename Class>
inline NativeCallback& NativeCallback::bind(void (Class::*method)(Params&))
{
    function_ = std::bind(method, static_cast<Class*>(runtime_), std::placeholders::_1);
    return *this;
}

inline bool NativeCallback::isNamed() const
{
    return !names_.empty();
}

inline const std::string& NativeCallback::getNameAt(size_t i) const
{
    return names_[i];
}

inline const void* NativeCallback::getDefaultAt(size_t i) const
{
    return defaults_[i];
}
// }}}

} // namespace FlowVM
} // namespace x0
