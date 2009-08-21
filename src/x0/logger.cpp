/* <x0/logger.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/logger.hpp>
#include <x0/strutils.hpp>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

// {{{ logger
logger::logger() :
	severity_(severity::warn)
{
}

logger::~logger()
{
}
// }}}

// {{{ nulllogger
nulllogger::nulllogger()
{
}

nulllogger::~nulllogger()
{
}

void nulllogger::cycle()
{
}

void nulllogger::write(severity /*s*/, const std::string& message)
{
}

nulllogger *nulllogger::clone() const
{
	return new nulllogger();
}
// }}}

// {{{ syslogger
syslogger::syslogger()
{
}

syslogger::~syslogger()
{
}

void syslogger::cycle()
{
}

void syslogger::write(severity s, const std::string& message)
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

syslogger *syslogger::clone() const
{
	return new syslogger();
}

// }}}

} // namespace x0
