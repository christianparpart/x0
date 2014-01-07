#pragma once

#include <x0/Buffer.h>
#include <string>
#include <cstdint>

namespace x0 {

enum class FlowType {
	Void = 0,
	Boolean = 1,        // bool (int64)
	Number = 2,         // int64
	String = 3,         // std::string* (TODO: later BufferRef)
	IPAddress = 5,      // IPAddress*
	Cidr = 6,           // Cidr*
	RegExp = 7,         // RegExp*
	Handler = 8,        // bool (*native_handler)(FlowContext*);
    Array = 9,          // array<V>
    AssocArray = 10,    // assocarray<K, V>
};

typedef uint64_t Register; // FlowVM

typedef int64_t FlowNumber;
typedef BufferRef FlowString;

std::string tos(FlowType type);

} // namespace x0

namespace std {
    template<> struct hash<x0::FlowType> {
        uint32_t operator()(x0::FlowType v) const noexcept { return static_cast<uint32_t>(v); }
    };
}
