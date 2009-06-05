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

// {{{ filelogger
filelogger::filelogger(const std::string& filename) :
	filename_(filename),
	fd_(-1)
{
	cycle();
}

filelogger::~filelogger()
{
	if (fd_ != -1)
	{
		close(fd_);
		fd_ = -1;
	}
}

void filelogger::cycle()
{
	int fd2 = open(filename_.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);

	if (fd2 == -1)
	{
		write(severity::error, "Could not (re)open new logfile");
	}
	else
	{
		if (fd_ != -1)
		{
			close(fd_);
		}

		fd_ = fd2;
	}
}

static inline std::string _now()
{
	char buf[26];
	std::time_t ts = time(0);

	if (struct tm *tm = localtime(&ts))
	{
		if (strftime(buf, sizeof(buf), "%m/%d/%Y:%T %z", tm) != 0)
		{
			return buf;
		}
	}
	return "-";
}

void filelogger::write(severity s, const std::string& message)
{
	if (s <= level())
	{
		std::string line(fstringbuilder::format("[%s] [%s] %s\n", _now().c_str(), s.c_str(), message.c_str()));
		::write(fd_, line.c_str(), line.length());
	}
}

filelogger *filelogger::clone() const
{
	return new filelogger(filename_);
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
