/* <x0/Logger.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Logger.h>
#include <x0/DateTime.h>
#include <x0/strutils.h>
#include <cstring>
#include <sd-daemon.h>

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

void NullLogger::write(LogMessage& /*message*/)
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

void SystemLogger::write(LogMessage& message)
{
	if (message.severity() <= level()) {
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

		Buffer buf;
		buf << message;

		syslog(tr[message.severity()], "%s", buf.c_str());
	}
}

SystemLogger *SystemLogger::clone() const
{
	return new SystemLogger();
}

// }}}

// {{{ SystemdLogger
SystemdLogger::SystemdLogger()
{
}

SystemdLogger::~SystemdLogger()
{
}

void SystemdLogger::cycle()
{
}

void SystemdLogger::write(LogMessage& message)
{
	static const char *sd[] = {
		SD_ERR,
		SD_WARNING,
		SD_NOTICE,
		SD_INFO,
		SD_DEBUG,
		SD_DEBUG,
		SD_DEBUG,
		SD_DEBUG,
		SD_DEBUG,
		SD_DEBUG
	};

	if (message.severity() <= level()) {
		Buffer buf;
		buf << message;

		fprintf(stderr, "%s%s\n", sd[message.severity()], buf.c_str());
	}
}

SystemdLogger *SystemdLogger::clone() const
{
	return new SystemdLogger();
}
// }}}

// {{{ FileLogger
FileLogger::FileLogger(const std::string& filename, std::function<time_t()> now) :
	filename_(filename),
	fd_(-1),
	now_(now)
{
	cycle();
}

FileLogger::FileLogger(int fd, std::function<time_t()> now) :
	filename_(),
	fd_(fd),
	now_(now)
{
}

FileLogger::~FileLogger()
{
	if (fd_ != -1 && !filename_.empty()) {
		::close(fd_);
		fd_ = -1;
	}
}

int FileLogger::handle() const
{
	return fd_;
}

void FileLogger::cycle()
{
	if (filename_.empty())
		return;

	int fd2 = ::open(filename_.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE
#if defined(O_CLOEXEC)
			| O_CLOEXEC
#endif
			, 0644);

	if (fd2 < 0) {
		LogMessage msg(Severity::error, "Could not re-open new logfile");
		write(msg);
	} else {
#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
		fcntl(fd2, F_SETFD, fcntl(fd2, F_GETFD) | FD_CLOEXEC);
#endif
		if (fd_ != -1) {
			::close(fd_);
		}

		fd_ = fd2;
	}
}

inline void FileLogger::write(LogMessage& message)
{
	if (message.severity() <= level() && fd_ >= 0) {
		Buffer buf;
		DateTime ts(now_());

		// TODO let `ts.htlog_str()` render directly into `buf`
		buf << "[" << ts.htlog_str() << "] [" << message.severity().c_str() << "] " << message << "\n";

		int rv = ::write(fd_, buf.data(), buf.size());

		if (rv < 0) {
			perror("FileLogger.write");
		}
	}
}

inline FileLogger *FileLogger::clone() const
{
	return new FileLogger(filename_, now_);
}
// }}}

#if 0
// ConsoleLogger

		static AnsiColor::Type colors[] = {
			AnsiColor::Red | AnsiColor::Bold, // error
			AnsiColor::Yellow | AnsiColor::Bold, // warn
			AnsiColor::Green | AnsiColor::Bold, // notice
			AnsiColor::Green, // info
			static_cast<AnsiColor::Type>(0),
			AnsiColor::Cyan, // debug
		};

		Buffer buf;
		buf << AnsiColor::make(colors[msg.severity() + 3]);
		buf << msg;
		buf << AnsiColor::make(AnsiColor::Clear);

#endif

} // namespace x0
