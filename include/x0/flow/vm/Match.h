#pragma once

#include <x0/Api.h>
#include <x0/flow/vm/Instruction.h>
#include <x0/flow/vm/MatchClass.h>
#include <x0/flow/FlowType.h>
#include <x0/PrefixTree.h>
#include <x0/SuffixTree.h>
#include <x0/RegExp.h>
#include <sys/types.h>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace x0 {
namespace FlowVM {

struct X0_API MatchCaseDef {
    //!< offset into the string pool (or regexp pool) of the associated program.
    uint64_t label;
    //!< program offset into the associated handler
    uint64_t pc;

    MatchCaseDef() = default;
    MatchCaseDef(uint64_t l, uint64_t p) : label(l), pc(p) {}
};

struct X0_API MatchDef {
    size_t handlerId;
    MatchClass op;                      // == =^ =$ =~
    uint64_t elsePC;
    std::vector<MatchCaseDef> cases;
};

class Program;
class Handler;
class Runner;

class X0_API Match {
public:
    Match(const MatchDef& def, Program* program);
    virtual ~Match();

    const MatchDef& def() const { return def_; }

    /**
     * Matches input condition.
     * \return a code pointer to continue processing
     */
    virtual uint64_t evaluate(const FlowString* condition, Runner* env) const = 0;

protected:
    MatchDef def_;
    Program* program_;
    Handler* handler_;
    uint64_t elsePC_;
};

/** Implements SMATCHEQ instruction. */
class X0_API MatchSame : public Match {
public:
    MatchSame(const MatchDef& def, Program* program);
    ~MatchSame();

    virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

private:
    std::unordered_map<FlowString, uint64_t> map_;
};

/** Implements SMATCHBEG instruction. */
class X0_API MatchHead : public Match {
public:
    MatchHead(const MatchDef& def, Program* program);
    ~MatchHead();

    virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

private:
    PrefixTree<FlowString, uint64_t> map_;
};

/** Implements SMATCHBEG instruction. */
class X0_API MatchTail : public Match {
public:
    MatchTail(const MatchDef& def, Program* program);
    ~MatchTail();

    virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

private:
    SuffixTree<FlowString, uint64_t> map_;
};

/** Implements SMATCHR instruction. */
class X0_API MatchRegEx : public Match {
public:
    MatchRegEx(const MatchDef& def, Program* program);
    ~MatchRegEx();

    virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

private:
    std::vector<std::pair<RegExp*, uint64_t>> map_;
};

} // namespace FlowVM
} // namespace x0
