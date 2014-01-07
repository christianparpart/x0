#pragma once

#include <x0/Api.h>
#include <x0/flow/vm/Instruction.h>
#include <x0/flow/FlowType.h>           // FlowNumber
#include <x0/RegExp.h>
#include <x0/IPAddress.h>
#include <x0/Cidr.h>

#include <vector>
#include <utility>
#include <memory>

namespace x0 {

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
        const std::vector<Cidr>& cidrs,
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

    FlowNumber number(size_t index) const { return numbers_[index]; }
    const FlowString& string(size_t index) const { return strings_[index].second; }
    const IPAddress& ipaddr(size_t index) const { return ipaddrs_[index]; }
    const Cidr& cidr(size_t index) const { return cidrs_[index]; }
    const RegExp* regularExpression(size_t index) const { return regularExpressions_[index]; }
    const Match* match(size_t index) const { return matches_[index]; }
    Handler* handler(size_t index) const { return handlers_[index]; }
    NativeCallback* nativeHandler(size_t index) const { return nativeHandlers_[index]; }
    NativeCallback* nativeFunction(size_t index) const { return nativeFunctions_[index]; }

    inline const std::vector<FlowNumber>& numbers() const { return numbers_; }
    inline const std::vector<std::pair<std::string, FlowString>>& strings() const { return strings_; }
    inline const std::vector<IPAddress>& ipaddrs() const { return ipaddrs_; }
    inline const std::vector<RegExp*>& regularExpressions() const { return regularExpressions_; }
    const std::vector<Match*>& matches() const { return matches_; }
    inline const std::vector<Handler*> handlers() const { return handlers_; }

    Handler* createHandler(const std::string& name);
    Handler* createHandler(const std::string& name, const std::vector<Instruction>& instructions);
    Handler* findHandler(const std::string& name) const;
    int indexOf(const Handler* handler) const;

    bool link(Runtime* runtime);

    void dump();

private:
    void setup(const std::vector<MatchDef>& matches);

private:
    std::vector<FlowNumber> numbers_;
    std::vector<std::pair<std::string, FlowString>> strings_;
    std::vector<IPAddress> ipaddrs_;
    std::vector<Cidr> cidrs_;
    std::vector<RegExp*> regularExpressions_;
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
