#include <x0/flow/VMCodeGenerator.h>
#include <x0/flow/vm/Program.h>

namespace x0 {

VMCodeGenerator::VMCodeGenerator() :
    errors_(),
    constNumbers_(),
    constStrings_(),
    ipaddrs_(),
    cidrs_(),
    regularExpressions_(),
    matches_(),
    modules_(),
    nativeHandlerSignatures_(),
    nativeFunctionSignatures_(),
    handlers_()
{
}

VMCodeGenerator::~VMCodeGenerator()
{
}

std::unique_ptr<FlowVM::Program> VMCodeGenerator::generate(IRProgram* program)
{
    // TODO
    return nullptr;
}


} // namespace x0
