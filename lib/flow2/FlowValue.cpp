#include <x0/flow2/FlowValue.h>

namespace x0 {

std::string FlowValue::asString() const
{
	switch (type()) {
		case FlowType::Void:
			return "(void)";
		case FlowType::Boolean:
			return toBoolean() ? "true" : "false";
		case FlowType::Number: {
			char buf[64];
			ssize_t n = snprintf(buf, sizeof(buf), "%llu", toNumber());
			return n >= 0 ? std::string(buf, buf + n) : std::string();
		}
		case FlowType::String:
		   return toString();
		case FlowType::Buffer:
		case FlowType::IPAddress:
		case FlowType::Cidr:
		case FlowType::RegExp:
		case FlowType::Array:
		case FlowType::Handler:
			return "TODO";
	}
}

} // namespace x0
