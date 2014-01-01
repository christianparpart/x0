#include <x0/flow/vm/Match.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/vm/Handler.h>
#include <x0/flow/vm/Runner.h>

namespace x0 {
namespace FlowVM {

// {{{ Match
Match::Match(const MatchDef& def, Program* program) :
    def_(def),
    program_(program),
    handler_(program->handler(def.handlerId))
{
}

Match::~Match()
{
}
// }}} Match
// {{{ MatchSame
MatchSame::MatchSame(const MatchDef& def, Program* program) :
    Match(def, program),
    map_()
{
    for (const auto& one: def.cases) {
        map_[program->strings()[one.label]] = one.pc;
    }
}

MatchSame::~MatchSame()
{
}

uint64_t MatchSame::evaluate(const FlowString* condition) const
{
    const auto i = map_.find(*condition);
    if (i != map_.end())
        return i->second;

    return def_.elsePC; // no match found
}
// }}}
// {{{ MatchHead
MatchHead::MatchHead(const MatchDef& def, Program* program) :
    Match(def, program),
    map_()
{
    for (const auto& one: def.cases) {
        map_.insert(program->strings()[one.label], one.pc);
    }
}

MatchHead::~MatchHead()
{
}

uint64_t MatchHead::evaluate(const FlowString* condition) const
{
    uint64_t result;
    if (map_.lookup(*condition, &result))
        return result;

    return def_.elsePC; // no match found
}
// }}}
// {{{ MatchTail
MatchTail::MatchTail(const MatchDef& def, Program* program) :
    Match(def, program),
    map_()
{
    for (const auto& one: def.cases) {
        map_.insert(program->strings()[one.label], one.pc);
    }
}

MatchTail::~MatchTail()
{
}

uint64_t MatchTail::evaluate(const FlowString* condition) const
{
    uint64_t result;
    if (map_.lookup(*condition, &result))
        return result;

    return def_.elsePC; // no match found
}
// }}}

} // namespace FlowVM
} // namespace x0
