#pragma once

#include <x0/Api.h>

namespace x0 {

class IRHandler;

class X0_API HandlerPass {
public:
    virtual ~HandlerPass() {}

    /**
     * Runs this pass on given \p handler.
     *
     * @retval true The handler was modified.
     * @retval false The handler was not modified.
     */
    virtual bool run(IRHandler* handler) = 0;
};

} // namespace x0
