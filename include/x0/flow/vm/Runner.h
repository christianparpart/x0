#pragma once

#include <x0/flow/FlowType.h>
#include <x0/flow/vm/Handler.h>
#include <x0/flow/vm/Instruction.h>
#include <utility>
#include <list>
#include <memory>
#include <new>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

namespace x0 {
namespace FlowVM {

// ExecutionEngine
// VM
class X0_API Runner
{
private:
    Handler* handler_;
    Program* program_;
    void* userdata_;        //!< pointer to the currently executed request handler in our case

    std::list<Buffer> stringGarbage_;
    std::list<Object*> garbage_;

    Register data_[];

public:
    ~Runner();

    static std::unique_ptr<Runner> create(Handler* handler);
    static void operator delete (void* p);

    bool run();

    Handler* handler() const { return handler_; }
    Program* program() const { return program_; }
    void* userdata() const { return userdata_; }
    void setUserData(void* p) { userdata_ = p; }

    FlowString* newString(const std::string& value);
    FlowString* newString(const char* p, size_t n);
    FlowString* catString(const FlowString& a, const FlowString& b);
    const FlowString* emptyString() const { return &*stringGarbage_.begin(); }

private:
    explicit Runner(Handler* handler);
    Runner(Runner&) = delete;
    Runner& operator=(Runner&) = delete;
};

} // namespace FlowVM
} // namespace x0
