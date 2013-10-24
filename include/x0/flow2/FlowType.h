#pragma once

#include <x0/Api.h>
#include <string>

namespace x0 {

enum class FlowType {
	Void = 0,
	Boolean = 1,    // bool (uint8)
	Number = 2,     // int64
	String = 3,     // C-String
	Buffer = 4,     // BufferRef
	IPAddress = 5,  // IPAddress*
	Cidr = 6,       // Cidr*
	RegExp = 7,     // RegExp*
	Array = 8,      // FlowValue[]
	Handler = 9,    // bool (*native_handler)(FlowContext*);
};

X0_API std::string tos(FlowType type);

} // namespace x0
