#pragma once

#include <x0/Api.h>

namespace x0 {

class IRHandler;

class X0_API HandlerPass {
public:
    explicit HandlerPass(const char* name) :
        name_(name) {}

    virtual ~HandlerPass() {}

    /**
     * Runs this pass on given \p handler.
     *
     * @retval true The handler was modified.
     * @retval false The handler was not modified.
     */
    virtual bool run(IRHandler* handler) = 0;

    const char* name() const { return name_; }

private:
    const char* name_;
};

} // namespace x0
