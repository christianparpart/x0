#pragma once

#include <x0/Api.h>
#include <list>
#include <memory>

namespace x0 {

class HandlerPass;
class IRHandler;
class IRProgram;

class X0_API PassManager {
public:
    PassManager();
    ~PassManager();

    /** registers given pass to the pass manager.
     */
    void registerPass(std::unique_ptr<HandlerPass>&& handlerPass);

    /** runs passes on a complete program.
     */
    void run(IRProgram* program);

    /** runs passes on given handler.
     */
    void run(IRHandler* handler);

private:
    std::list<std::unique_ptr<HandlerPass>> handlerPasses_;
};

} // namespace x0
