#pragma once

#include <x0/Buffer.h>
#include <string>
#include <vector>
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
    IntArray = 9,       // array<int>
    StringArray = 10,   // array<string>
};

typedef uint64_t Register; // FlowVM

typedef int64_t FlowNumber;
typedef BufferRef FlowString;

std::string tos(FlowType type);

class X0_API Object {
public:
    virtual ~Object() {}
};

template<typename T>
class X0_API GCObject : public Object {
private:
    T data_;

public:
    template<typename... Args>
    explicit GCObject(Args&&... args) :
        data_(args...)
    {}

    T& data() { return data_; }
    const T& data() const { return data_; }
};

typedef GCObject<std::vector<FlowNumber>> GCIntArray;
typedef GCObject<std::vector<FlowString>> GCStringArray;

} // namespace x0

namespace std {
    template<> struct hash<x0::FlowType> {
        uint32_t operator()(x0::FlowType v) const noexcept { return static_cast<uint32_t>(v); }
    };
}
