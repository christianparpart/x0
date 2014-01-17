#pragma once

#include <x0/Api.h>
#include <x0/flow/FlowType.h>
#include <x0/flow/vm/Handler.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/vm/Runner.h>
#include <x0/IPAddress.h>

namespace x0 {

class Cidr;

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
    void setResult(FlowNumber value) { argv_[0] = (Register) value; }
    void setResult(Handler* handler) { argv_[0] = caller_->program()->indexOf(handler); }
    void setResult(const char* cstr) { argv_[0] = (Register) caller_->newString(cstr); }
    void setResult(const std::string& str) { argv_[0] = (Register) caller_->newString(str.data(), str.size()); }
    void setResult(const FlowString& str) { argv_[0] = (Register) &str; }
    void setResult(const FlowString* str) { argv_[0] = (Register) str; }
    void setResult(const IPAddress* ip) { argv_[0] = (Register) ip; }
    void setResult(const Cidr* cidr) { argv_[0] = (Register) cidr; }

    int size() const { return argc_; }
    int count() const { return argc_; }

    Register at(size_t i) const { return argv_[i]; }
    Register operator[](size_t i) const { return argv_[i]; }
    Register& operator[](size_t i) { return argv_[i]; }

    template<typename T> T get(size_t offset) const;

    class iterator { // {{{
    private:
        Params* params_;
        size_t current_;

    public:
        iterator(Params* p, size_t init) : params_(p), current_(init) {}
        iterator(const iterator& v) = default;

        size_t offset() const { return current_; }
        Register get() const { return params_->at(current_); }

        Register& operator*() { return params_->argv_[current_]; }
        const Register& operator*() const { return params_->argv_[current_]; }

        iterator& operator++() {
            if (current_ != params_->argc_) {
                ++current_;
            }
            return *this;
        }

        bool operator==(const iterator& other) const {
            return current_ == other.current_;
        }

        bool operator!=(const iterator& other) const {
            return current_ != other.current_;
        }
    }; // }}}

    iterator begin() { return iterator(this, std::min(1, argc_)); }
    iterator end() { return iterator(this, argc_); }
};

template<> X0_API inline bool Params::get<bool>(size_t offset) const { return at(offset); }
template<> X0_API inline FlowNumber Params::get<FlowNumber>(size_t offset) const { return at(offset); }
template<> X0_API inline FlowString Params::get<FlowString>(size_t offset) const { return *((FlowString*) at(offset)); }
template<> X0_API inline FlowString* Params::get<FlowString*>(size_t offset) const { return ((FlowString*) at(offset)); }
template<> X0_API inline Handler* Params::get<Handler*>(size_t offset) const { return caller_->program()->handler(at(offset)); }
template<> X0_API inline IPAddress Params::get<IPAddress>(size_t offset) const { return *((IPAddress*) at(offset)); }
template<> X0_API inline IPAddress* Params::get<IPAddress*>(size_t offset) const { return ((IPAddress*) at(offset)); }
template<> X0_API inline GCStringArray* Params::get<GCStringArray*>(size_t offset) const { return ((GCStringArray*) at(offset)); }
template<> X0_API inline GCIntArray* Params::get<GCIntArray*>(size_t offset) const { return ((GCIntArray*) at(offset)); }

} // namespace FlowVM
} // namespace x0
