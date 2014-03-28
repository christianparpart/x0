/* <x0/flow/ir/IRHandler.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Constant.h>

#include <string>
#include <vector>
#include <list>

namespace x0 {

class BasicBlock;
class IRProgram;
class IRBuilder;

class X0_API IRHandler : public Constant {
public:
    explicit IRHandler(const std::string& name);
    ~IRHandler();

    BasicBlock* createBlock(const std::string& name = "");

    BasicBlock* entryPoint() const { return blocks_.front(); }

    IRProgram* parent() const { return parent_; }
    void setParent(IRProgram* prog) { parent_ = prog; }

    void dump() override;

    std::list<BasicBlock*>& basicBlocks() { return blocks_; }

    BasicBlock* getEntryBlock() const { return blocks_.front(); }

    /**
     * Removes given basic block \p bb from handler.
     */
    void remove(BasicBlock* bb);

    /**
     * Performs given transformation on this handler.
     *
     * @see HandlerPass
     */
    template<typename TheHandlerPass, typename... Args>
    size_t transform(Args&&... args) {
        return TheHandlerPass(args...).run(this);
    }

    /**
     * Performs sanity checks on internal data structures.
     *
     * This call does not return any success or failure as every failure is considered fatal
     * and will cause the program to exit with diagnostics as this is most likely caused by
     * an application programming error.
     *
     * @note Always call this on completely defined handlers and never on half-contructed ones.
     */
    void verify();

private:
    IRProgram* parent_;
    std::list<BasicBlock*> blocks_;

    friend class IRBuilder;
};

} // namespace x0
