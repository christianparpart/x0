#include <x0/flow/vm/MatchClass.h>
#include <cassert>

namespace x0 {
namespace FlowVM {

std::string tos(MatchClass mc)
{
    switch (mc) {
        case MatchClass::Same:
            return "Same";
        case MatchClass::Head:
            return "Head";
        case MatchClass::Tail:
            return "Tail";
        case MatchClass::RegExp:
            return "RegExp";
        default:
            assert(!"FIXME: NOT IMPLEMENTED");
            return "<FIXME>";
    }
}

} // namespace FlowVM
} // namespace x0

