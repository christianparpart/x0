#include <x0/types.hpp>

namespace x0 {

severity::severity(const std::string& name)
{
	if (name == "emergency")
		value_ = emergency;
	else if (name == "alert")
		value_ = alert;
	else if (name == "critical")
		value_ = critical;
	else if (name == "error")
		value_ = error;
	else if (name == "warn" || name.empty()) // <- default: warn
		value_ = warn;
	else if (name == "notice")
		value_ = notice;
	else if (name == "info")
		value_ = info;
	else //if (name == "debug")
		value_ = debug;
}

} // namespace x0
