#pragma once

#include <x0/flow/vm/Instruction.h>
#include <x0/Api.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace x0 {
namespace FlowVM {

class Program;
class Runner;

class X0_API Handler
{
public:
    Handler();
    Handler(Program* program, const std::string& name,
        const std::vector<Instruction>& instructions);
    Handler(const Handler& handler);
    Handler(Handler&& handler);
    ~Handler();

    Program* program() const { return program_; }

    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    size_t registerCount() const { return registerCount_; }

    const std::vector<Instruction>& code() const { return code_; }
    void setCode(const std::vector<Instruction>& code);
    void setCode(std::vector<Instruction>&& code);

    std::unique_ptr<Runner> createRunner();
    bool run(void* userdata = nullptr);

    void disassemble();

private:
    Program* program_;
    std::string name_;
    size_t registerCount_;
    std::vector<Instruction> code_;
};

} // namespace FlowVM
} // namespace x0
