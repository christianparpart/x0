#pragma once

#include <x0/Api.h>
#include <x0/flow2/FlowType.h>
#include <x0/flow2/vm/Handler.h>
#include <x0/flow2/vm/Program.h>
#include <x0/flow2/vm/Runner.h>

namespace x0 {
namespace FlowVM {

class X0_API Params {
private:
    int argc_;
    Register* argv_;
    Runner* caller_;

public:
    Params(int argc, Register* argv, Runner* caller) : argc_(argc), argv_(argv), caller_(caller) {}

    Runner* caller() const { return caller_; }

    void setResult(bool value) { argv_[0] = value; }
    void setResult(Register value) { argv_[0] = value; }
    void setResult(const char* cstr) { argv_[0] = (Register) caller_->createString(cstr); }
    void setResult(Handler* handler) { argv_[0] = caller_->program()->indexOf(handler); }

    int size() const { return argc_; }
    int count() const { return argc_; }

    Register at(size_t i) const { return argv_[i]; }
    Register operator[](size_t i) const { return argv_[i]; }
    Register& operator[](size_t i) { return argv_[i]; }

    template<typename T> T get(size_t offset) const;
};

template<> X0_API inline bool Params::get<bool>(size_t offset) const { return at(offset); }
template<> X0_API inline FlowNumber Params::get<FlowNumber>(size_t offset) const { return at(offset); }
template<> X0_API inline FlowString Params::get<FlowString>(size_t offset) const { return *((FlowString*) at(offset)); }
template<> X0_API inline FlowString* Params::get<FlowString*>(size_t offset) const { return ((FlowString*) at(offset)); }
template<> X0_API inline Handler* Params::get<Handler*>(size_t offset) const { return caller_->program()->handler(at(offset)); }

} // namespace FlowVM
} // namespace x0
