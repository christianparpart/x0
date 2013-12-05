/* 
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/flow2/FlowValue.h>
#include <x0/flow2/FlowVM.h>
#include <x0/flow2/FlowProgram.h>
#include <unordered_map>
#include <deque>
#include <vector>

namespace x0 {

class ASTNode;
class Symbol;
class FlowEngine;

// XXX we might want to add debug/runtime info here, that aids some diagnostics

class X0_API FlowContext {
public:
    enum Status {
        Done,
        Running,
        Interrupted,
    };

private:
    Status status_;

    size_t programSize_;
    const uint8_t* program_;
    size_t pc_;

    size_t stackSize_;
    uint64_t* stack_;
    size_t sp_;

    size_t dataSize_;
    uint64_t* data_;

    size_t localsSize_;
    uint64_t* locals_;

    // RegExp::Match* regexpMatch_;

public:
    FlowContext();
    virtual ~FlowContext();

    void setProgram(const uint8_t* program, size_t programSize, size_t stackSize, size_t localsSize);
    void setProgram(const FlowProgram& program);

    void run();

    Status status() const { return status_; }
    FlowNumber result() const { return sp_ ? *((FlowNumber*) &stack_[sp_ - 1]) : 0; }

private:
    uint8_t read8() { return program_[pc_++]; }
    uint16_t read16() { return (read8() << 8) | read8(); }
    uint32_t read32() { return (((uint32_t)read16()) << 16) | read16(); }
    uint64_t read64() { return (((uint64_t)read32()) << 32) | read32(); }

    void push(FlowNumber value);
    void push(const char* buf, size_t length);
    template<typename T> T pop();

    void dumpInstr(bool lf = true);
};

inline void FlowContext::push(FlowNumber value) {
    stack_[sp_++] = value;
}

template<> inline FlowNumber FlowContext::pop<FlowNumber>()
{
    return stack_[--sp_];
}

} // namespace x0 
