#include <x0/flow/FlowValue.h>

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
			return "TODO (Buffer)";
		case FlowType::IPAddress:
			return "TODO (IPAddress)";
		case FlowType::Cidr:
			return "TODO (Cidr)";
		case FlowType::RegExp:
			return "TODO (RegExp)";
		case FlowType::Array:
			return "TODO (Array)";
		case FlowType::Handler:
			return "TODO (Handler)";
	}
}

} // namespace x0
