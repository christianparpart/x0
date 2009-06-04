#include <x0/logger.hpp>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
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

void filelogger::write(severity /*s*/, const std::string& message)
{
	::write(fd_, message.c_str(), message.length());
	::write(fd_, "\n", sizeof("\n"));
}

filelogger *filelogger::clone() const
{
	return new filelogger(filename_);
}
// }}}

} // namespace x0
