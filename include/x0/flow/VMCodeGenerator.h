#pragma once

#include <x0/Cidr.h>
#include <x0/IPAddress.h>
#include <x0/flow/FlowType.h>
#include <x0/flow/vm/Match.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <utility>
#include <memory>

namespace x0 {

class IRProgram;

namespace FlowVM {
    class Program;
}

class VMCodeGenerator {
public:
    VMCodeGenerator();
    ~VMCodeGenerator();

    std::unique_ptr<FlowVM::Program> generate(IRProgram* program);

private:
    std::vector<std::string> errors_;           //!< list of raised errors during code generation.

    // target program output
    std::vector<FlowNumber> constNumbers_;
    std::vector<std::string> constStrings_;
    std::vector<IPAddress> ipaddrs_;
    std::vector<Cidr> cidrs_;
    std::vector<std::string> regularExpressions_;
    std::vector<FlowVM::MatchDef> matches_;
    std::vector<std::pair<std::string, std::string>> modules_;
    std::vector<std::string> nativeHandlerSignatures_;
    std::vector<std::string> nativeFunctionSignatures_;
    std::vector<std::pair<std::string, std::vector<FlowVM::Instruction>>> handlers_;
};


} // namespace x0
