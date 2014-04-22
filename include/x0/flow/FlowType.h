#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <string>
#include <vector>
#include <cstdint>

namespace x0 {

class IPAddress;
class Cidr;

//! \addtogroup Flow
//@{

enum class FlowType {
    Void = 0,
    Boolean = 1,        // bool (int64)
    Number = 2,         // int64
    String = 3,         // BufferRef*
    IPAddress = 5,      // IPAddress*
    Cidr = 6,           // Cidr*
    RegExp = 7,         // RegExp*
    Handler = 8,        // bool (*native_handler)(FlowContext*);
    IntArray = 9,       // array<int>
    StringArray = 10,   // array<string>
    IPAddrArray = 11,   // array<IPAddress>
    CidrArray = 12,     // array<Cidr>
};

typedef uint64_t Register; // FlowVM

typedef int64_t FlowNumber;
typedef BufferRef FlowString;

std::string tos(FlowType type);

// {{{ array types
class X0_API FlowArray {
public:
    size_t size() const { return base_[0]; }

protected:
    explicit FlowArray(const Register* base) : base_(base) {}

    Register getRawAt(size_t i) const { return base_[1 + i]; }
    const Register* data() const { return base_ + 1; }

protected:
    const Register* base_;
};

typedef std::vector<FlowNumber> FlowIntArray;
typedef std::vector<FlowString> FlowStringArray;
typedef std::vector<IPAddress> FlowIPAddrArray;
typedef std::vector<Cidr> FlowCidrArray;

// }}}

//!@}

} // namespace x0

namespace std {
    template<> struct hash<x0::FlowType> {
        uint32_t operator()(x0::FlowType v) const noexcept { return static_cast<uint32_t>(v); }
    };
}
