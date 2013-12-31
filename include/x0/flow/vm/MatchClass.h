#pragma once

#include <x0/Api.h>
#include <string>

namespace x0 {
namespace FlowVM {

enum class MatchClass {
    Same,
    Head,
    Tail,
    RegExp,
};

X0_API std::string tos(MatchClass c);

} // namespace FlowVM
} // namespace x0

