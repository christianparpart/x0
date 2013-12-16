#include <x0/flow2/vm/Handler.h>
#include <x0/flow2/vm/Runner.h>
#include <x0/flow2/vm/Instruction.h>

namespace x0 {
namespace FlowVM {

Handler::Handler()
{
}

Handler::Handler(Program* program, const std::string& name,
        const std::vector<Instruction>& code) :
    program_(program),
    name_(name),
    registerCount_(computeRegisterCount(code.data(), code.size())),
    code_(code)
{
}

Handler::Handler(const Handler& v) :
    program_(v.program_),
    name_(v.name_),
    registerCount_(v.registerCount_),
    code_(v.code_)
{
}

Handler::Handler(Handler&& v) :
    program_(std::move(v.program_)),
    name_(std::move(v.name_)),
    registerCount_(std::move(v.registerCount_)),
    code_(std::move(v.code_))
{
}

Handler::~Handler()
{
}

void Handler::setCode(const std::vector<Instruction>& code)
{
    code_ = code;
    registerCount_ = computeRegisterCount(code_.data(), code_.size());
}

void Handler::setCode(std::vector<Instruction>&& code)
{
    code_ = std::move(code);
    registerCount_ = computeRegisterCount(code_.data(), code_.size());
}

std::unique_ptr<Runner> Handler::createRunner()
{
    return Runner::create(this);
}

bool Handler::run(void* userdata)
{
    auto runner = createRunner();
    runner->setUserData(userdata);
    return runner->run();
}

void Handler::disassemble()
{
    FlowVM::disassemble(code_.data(), code_.size());
}

} // namespace FlowVM
} // namespace x0
