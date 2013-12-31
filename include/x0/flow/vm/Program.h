#pragma once

#include <x0/Api.h>
#include <x0/flow/vm/Instruction.h>
#include <x0/flow/FlowType.h>           // FlowNumber

#include <vector>
#include <utility>
#include <memory>

namespace x0 {

class IPAddress;

namespace FlowVM {

class Runner;
class Runtime;
class Handler;
class Match;
class MatchDef;
class NativeCallback;

class X0_API Program
{
public:
    Program();
    Program(
        const std::vector<FlowNumber>& constNumbers,
        const std::vector<std::string>& constStrings,
        const std::vector<IPAddress>& ipaddrs,
        const std::vector<std::string>& regularExpressions,
        const std::vector<MatchDef>& matches,
        const std::vector<std::pair<std::string, std::string>>& modules,
        const std::vector<std::string>& nativeHandlerSignatures,
        const std::vector<std::string>& nativeFunctionSignatures,
        const std::vector<std::pair<std::string, std::vector<Instruction>>>& handlers
    );
    Program(Program&) = delete;
    Program& operator=(Program&) = delete;
    ~Program();

    inline const std::vector<FlowNumber>& numbers() const { return numbers_; }
    inline const std::vector<FlowString>& strings() const { return strings_; }
    inline const std::vector<IPAddress>& ipaddrs() const { return ipaddrs_; }
    inline const std::vector<std::string>& regularExpressions() const { return regularExpressions_; }
    const std::vector<Match*>& matches() const { return matches_; }
    inline const std::vector<Handler*> handlers() const { return handlers_; }

    Handler* createHandler(const std::string& name);
    Handler* createHandler(const std::string& name, const std::vector<Instruction>& instructions);
    Handler* findHandler(const std::string& name) const;
    Handler* handler(size_t index) const { return handlers_[index]; }
    int indexOf(const Handler* handler) const;

    NativeCallback* nativeHandler(size_t id) const { return nativeHandlers_[id]; }
    NativeCallback* nativeFunction(size_t id) const { return nativeFunctions_[id]; }

    bool link(Runtime* runtime);

    void dump();

private:
    void setup(const std::vector<MatchDef>& matches);

private:
    std::vector<FlowNumber> numbers_;
    std::vector<FlowString> strings_;
    std::vector<IPAddress> ipaddrs_;
    std::vector<std::string> regularExpressions_;               // XXX to be a pre-compiled handled during runtime
    std::vector<Match*> matches_;
    std::vector<std::pair<std::string, std::string>> modules_;
    std::vector<std::string> nativeHandlerSignatures_;
    std::vector<std::string> nativeFunctionSignatures_;

    std::vector<NativeCallback*> nativeHandlers_;
    std::vector<NativeCallback*> nativeFunctions_;
    std::vector<Handler*> handlers_;
    Runtime* runtime_;
};

} // namespace FlowVM
} // namespace x0
