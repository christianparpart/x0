#pragma once

#include <x0/Api.h>

namespace x0 {

enum class FlowType {
	Void,
	Boolean,		// int8 (bool)
	Number,			// int64
	String,			// C-String
	Buffer,			// BufferRef
	IPAddress,		// IPv4 & IPv6
	IPNetwork,		// in CIDR notation
	RegExp,			// x0::RegExp*
	Array,			// FlowValue[]
	Handler,		// bool (*native_handler)(FlowContext*);
};

} // namespace x0
