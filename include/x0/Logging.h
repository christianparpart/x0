/* <Logging.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

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
	virtual ~Logging() {}

	void setLoggingPrefix(const char *prefix, ...);
	void setLogging(bool enable);

	const std::string& loggingPrefix() const { return prefix_; }

	void debug(const char *fmt, ...);

private:
	static void initialize();
	static void finalize();
};

} // namespace x0

#endif
