#include <x0/flow2/FlowType.h>

namespace x0 {

std::string tos(FlowType type)
{
	switch (type) {
		case FlowType::Void: return "void";
		case FlowType::Boolean: return "bool";
		case FlowType::Number: return "int";
		case FlowType::String: return "string";
		case FlowType::IPAddress: return "IPAddress";
		case FlowType::Cidr: return "Cidr";
		case FlowType::RegExp: return "RegExp";
		case FlowType::Handler: return "HandlerRef";
		case FlowType::Array: return "Array";
		case FlowType::AssocArray: return "AssocArray";
		default:
			return "";
	}
}


} // namespace x0
