#include <x0/flow/vm/Handler.h>
#include <x0/flow/vm/Runner.h>
#include <x0/flow/vm/Instruction.h>

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
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
    , directThreadedCode_()
#endif
{
}

Handler::Handler(const Handler& v) :
    program_(v.program_),
    name_(v.name_),
    registerCount_(v.registerCount_),
    code_(v.code_)
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
    , directThreadedCode_(v.directThreadedCode_)
#endif
{
}

Handler::Handler(Handler&& v) :
    program_(std::move(v.program_)),
    name_(std::move(v.name_)),
    registerCount_(std::move(v.registerCount_)),
    code_(std::move(v.code_))
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
    , directThreadedCode_(std::move(v.directThreadedCode_))
#endif
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

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
    directThreadedCode_.clear();
#endif
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
