/* <x0/Logger.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Logger.h>
#include <x0/strutils.h>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

// {{{ Logger
Logger::Logger() :
	severity_(Severity::warn)
{
}

Logger::~Logger()
{
}
// }}}

// {{{ NullLogger
NullLogger::NullLogger()
{
}

NullLogger::~NullLogger()
{
}

void NullLogger::cycle()
{
}

void NullLogger::write(Severity /*s*/, const std::string& message)
{
}

NullLogger *NullLogger::clone() const
{
	return new NullLogger();
}
// }}}

// {{{ SystemLogger
SystemLogger::SystemLogger()
{
}

SystemLogger::~SystemLogger()
{
}

void SystemLogger::cycle()
{
}

void SystemLogger::write(Severity s, const std::string& message)
{
	if (s <= level())
	{
		static int tr[] = {
			LOG_DEBUG,
			LOG_INFO,
			LOG_NOTICE,
			LOG_WARNING,
			LOG_ERR,
			LOG_CRIT,
			LOG_ALERT,
			LOG_EMERG
		};

		syslog(tr[s], "%s", message.c_str());
	}
}

SystemLogger *SystemLogger::clone() const
{
	return new SystemLogger();
}

// }}}

} // namespace x0
