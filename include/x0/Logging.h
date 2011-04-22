#ifndef sw_x0_Logging_h
#define sw_x0_Logging_h

#include <x0/Api.h>
#include <string>

namespace x0 {

/** This class is to be inherited by classes that need deeper inspections when logging.
 */
class X0_API Logging
{
private:
	std::string prefix_;
	bool enabled_;

public:
	Logging();
	explicit Logging(const char *prefix, ...);

	void setLoggingPrefix(const char *prefix, ...);
	void setLogging(bool enable);

	void debug(const char *fmt, ...);
};

} // namespace x0

#endif
