#include <x0/flow2/FlowType.h>

namespace x0 {

std::string tos(FlowType type) {
	switch (type) {
		case FlowType::Void: return "void";
		case FlowType::Boolean: return "bool";
		case FlowType::Number: return "num";
		case FlowType::String: return "str";
		case FlowType::Buffer: return "buf";
		case FlowType::IPAddress: return "ip";
		case FlowType::Cidr: return "cidr";
		case FlowType::RegExp: return "regex";
		case FlowType::Array: return "array";
		case FlowType::Handler: return "handlerref";
		default:
			return "";
	}
}


} // namespace x0
