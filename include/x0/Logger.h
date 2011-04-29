/* <Logger.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_errorlog_h
#define sw_x0_errorlog_h

#include <x0/Buffer.h>
#include <x0/FixedBuffer.h>
#include <x0/Severity.h>
#include <x0/Types.h>
#include <x0/Api.h>
#include <string>
#include <memory>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

//! \addtogroup base
//@{

/** logging facility.
 *
 * \see FileLogger
 */
class Logger
{
	Logger& operator=(const Logger&) = delete;
	Logger(const Logger&) = delete;

public:
	Logger();
	virtual ~Logger();

	/** reallocates resources used by this logger. */
	virtual void cycle() = 0;

	/** writes a message into the logger. */
	virtual void write(Severity s, const std::string& message) = 0;

	/** duplicates (clones) this logger. */
	virtual Logger *clone() const = 0;

	/** retrieves the loggers Severity level */
	Severity level() const { return severity_; }

	/** sets the loggers Severity level */
	void level(Severity value) { severity_ = value; }

	template<typename A0, typename... Args>
	void write(Severity s, const char *fmt, A0&& a0, Args&&... args)
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), fmt, a0, args...);
		this->write(s, std::string(buf));
	}

private:
	Severity severity_;
};

typedef std::shared_ptr<Logger> LoggerPtr;

/** implements a NULL logger (logs nothing).
 *
 * \see logger, FileLogger
 */
class NullLogger :
	public Logger
{
public:
	NullLogger();
	~NullLogger();

	virtual void cycle();
	virtual void write(Severity s, const std::string& message);
	virtual NullLogger *clone() const;
};

/** implements a file based logger.
 *
 * \see logger, server
 */
template<class Now>
class FileLogger :
	public Logger
{
public:
	FileLogger(const std::string& filename, Now now);
	~FileLogger();

	virtual void cycle();
	virtual void write(Severity s, const std::string& message);
	virtual FileLogger *clone() const;

	int handle() const;

private:
	std::string filename_;
	int fd_;
	Now now_;
};

/** implements a file based logger.
 *
 * \see logger, server
 */
class SystemLogger :
	public Logger
{
public:
	SystemLogger();
	~SystemLogger();

	virtual void cycle();
	virtual void write(Severity s, const std::string& message);
	virtual SystemLogger *clone() const;
};

/** implements a file based logger.
 *
 * \see logger, server
 */
class SystemdLogger :
	public Logger
{
public:
	SystemdLogger();
	~SystemdLogger();

	virtual void cycle();
	virtual void write(Severity s, const std::string& message);
	virtual SystemdLogger *clone() const;
};

// {{{ FileLogger
template<typename Now>
inline FileLogger<Now>::FileLogger(const std::string& filename, Now now) :
	filename_(filename),
	fd_(-1),
	now_(now)
{
	cycle();
}

template<typename Now>
inline FileLogger<Now>::~FileLogger()
{
	if (fd_ != -1)
	{
		::close(fd_);
		fd_ = -1;
	}
}

template<typename Now>
inline int FileLogger<Now>::handle() const
{
	return fd_;
}

template<typename Now>
inline void FileLogger<Now>::cycle()
{
	int fd2 = ::open(filename_.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE
#if defined(O_CLOEXEC)
			| O_CLOEXEC
#endif
			, 0644);

	if (fd2 == -1)
	{
		write(Severity::error, "Could not (re)open new logfile");
	}
	else
	{
#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
		fcntl(fd2, F_SETFD, fcntl(fd2, F_GETFD) | FD_CLOEXEC);
#endif
		if (fd_ != -1)
		{
			::close(fd_);
		}

		fd_ = fd2;
	}
}

template<typename Now>
inline void FileLogger<Now>::write(Severity s, const std::string& message)
{
	if (s <= level())
	{
		FixedBuffer<4096> buf;
		buf.push_back('[');
		buf.push_back(now_());
		buf.push_back("] [");
		buf.push_back(s.c_str());
		buf.push_back("] ");
		buf.push_back(message);
		buf.push_back('\n');

		(void) ::write(fd_, buf.data(), buf.size());
	}
}

template<typename Now>
inline FileLogger<Now> *FileLogger<Now>::clone() const
{
	return new FileLogger(filename_, now_);
}
// }}}

//@}

} // namespace x0

#endif
