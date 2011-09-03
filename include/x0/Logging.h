#ifndef sw_x0_Logging_h
#define sw_x0_Logging_h

#include <x0/Api.h>
#include <vector>
#include <string>

namespace x0 {

/** This class is to be inherited by classes that need deeper inspections when logging.
 */
class X0_API Logging
{
private:
	static std::vector<char*> env_;

	std::string prefix_;
	std::string className_;
	bool enabled_;

	void updateClassName();
	bool checkEnabled();

public:
	Logging();
	explicit Logging(const char *prefix, ...);

	void setLoggingPrefix(const char *prefix, ...);
	void setLogging(bool enable);

	void debug(const char *fmt, ...);

private:
	static void initialize();
	static void finalize();
};

} // namespace x0

#endif
