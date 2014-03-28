#pragma once

#include <x0/Api.h>
#include <x0/RegExp.h>
#include <x0/Cidr.h>
#include <x0/IPAddress.h>
#include <x0/Buffer.h>
#include <x0/flow/FlowType.h>
#include <x0/flow/vm/Match.h>

namespace x0 {
namespace FlowVM {

/**
 * Provides a pool of constant that can be built dynamically during code generation and accessed effeciently at runtime.
 *
 * @see Program
 */
struct X0_API ConstantPool {
public:
    ConstantPool(const ConstantPool& v) = delete;
    ConstantPool& operator=(const ConstantPool& v) = delete;

    ConstantPool();
    ConstantPool(ConstantPool&& from);
    ConstantPool& operator=(ConstantPool&& v);
    ~ConstantPool();

    // builder
    size_t makeInteger(FlowNumber value);
    size_t makeString(const std::string& value);
    size_t makeIPAddress(const IPAddress& value);
    size_t makeCidr(const Cidr& value);
    size_t makeRegExp(const RegExp& value);

    size_t makeIntegerArray(const std::vector<FlowNumber>& elements);
    size_t makeStringArray(const std::vector<std::string>& elements);
    size_t makeIPaddrArray(const std::vector<IPAddress>& elements);
    size_t makeCidrArray(const std::vector<Cidr>& elements);

    size_t makeMatchDef();
    MatchDef& getMatchDef(size_t id) { return matchDefs_[id]; }

    size_t makeNativeHandler(const std::string& sig);
    size_t makeNativeFunction(const std::string& sig);

    size_t makeHandler(const std::string& name);

    void setModules(const std::vector<std::pair<std::string, std::string>>& modules) { modules_ = modules; }

    // accessor
    FlowNumber getInteger(size_t id) const { return numbers_[id]; }
    const FlowString& getString(size_t id) const { return strings_[id]; }
    const IPAddress& getIPAddress(size_t id) const { return ipaddrs_[id]; }
    const Cidr& getCidr(size_t id) const { return cidrs_[id]; }
    const RegExp& getRegExp(size_t id) const { return regularExpressions_[id]; }

    const std::vector<FlowNumber>& getIntArray(size_t id) const { return intArrays_[id]; }
    const std::vector<BufferRef>& getStringArray(size_t id) const { return stringArrays_[id].second; }
    const std::vector<IPAddress>& getIPAddressArray(size_t id) const { return ipaddrArrays_[id]; }
    const std::vector<Cidr>& getCidrArray(size_t id) const { return cidrArrays_[id]; }

    const MatchDef& getMatchDef(size_t id) const { return matchDefs_[id]; }

    const std::pair<std::string, std::vector<Instruction>>& getHandler(size_t id) const { return handlers_[id]; }
    std::pair<std::string, std::vector<Instruction>>& getHandler(size_t id) { return handlers_[id]; }

    // bulk accessors
    const std::vector<std::pair<std::string, std::string>>& getModules() const { return modules_; }
    const std::vector<std::pair<std::string, std::vector<Instruction>>>& getHandlers() const { return handlers_; }
    const std::vector<MatchDef>& getMatchDefs() const { return matchDefs_; }
    const std::vector<std::string>& getNativeHandlerSignatures() const { return nativeHandlerSignatures_; }
    const std::vector<std::string>& getNativeFunctionSignatures() const { return nativeFunctionSignatures_; }

    void dump() const;

private:
    // constant primitives
    std::vector<FlowNumber> numbers_;
    std::vector<Buffer> strings_;
    std::vector<IPAddress> ipaddrs_;
    std::vector<Cidr> cidrs_;
    std::vector<RegExp> regularExpressions_;

    // constant arrays
    std::vector<std::vector<FlowNumber>> intArrays_;
    std::vector<std::pair<std::vector<Buffer>, std::vector<BufferRef>>> stringArrays_;
    std::vector<std::vector<IPAddress>> ipaddrArrays_;
    std::vector<std::vector<Cidr>> cidrArrays_;

    // code data
    std::vector<std::pair<std::string, std::string>> modules_;
    std::vector<std::pair<std::string, std::vector<Instruction>>> handlers_;
    std::vector<MatchDef> matchDefs_;
    std::vector<std::string> nativeHandlerSignatures_;
    std::vector<std::string> nativeFunctionSignatures_;
};

} // namespace FlowVM
} // namespace x0
