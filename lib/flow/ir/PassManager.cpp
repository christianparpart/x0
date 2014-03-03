#include <x0/flow/ir/PassManager.h>
#include <x0/flow/ir/HandlerPass.h>
#include <x0/flow/ir/IRProgram.h>

namespace x0 {

PassManager::PassManager()
{
}

PassManager::~PassManager()
{
}

void PassManager::registerPass(std::unique_ptr<HandlerPass>&& handlerPass)
{
    handlerPasses_.push_back(std::move(handlerPass));
}

void PassManager::run(IRProgram* program)
{
    for (IRHandler* handler: program->handlers()) {
        run(handler);
    }
}

void PassManager::run(IRHandler* handler)
{
again:
    for (auto& pass: handlerPasses_) {
        if (pass->run(handler)) {
            goto again;
        }
    }
}

} // namespace x0
