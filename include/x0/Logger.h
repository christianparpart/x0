/* <Logger.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_errorlog_h
#define sw_x0_errorlog_h

#include <x0/Buffer.h>
#include <x0/Severity.h>
#include <x0/LogMessage.h>
#include <x0/Types.h>
#include <x0/Api.h>
#include <cstdio>
#include <string>
#include <memory>
#include <functional>
#include <ctime>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace x0 {

//! \addtogroup base
//@{

/** logging facility.
 *
 * \see FileLogger
 */
class X0_API Logger
{
    Logger& operator=(const Logger&) = delete;
    Logger(const Logger&) = delete;

public:
    Logger();
    virtual ~Logger();

    /** reallocates resources used by this logger. */
    virtual void cycle() = 0;

    /** writes a message into the logger. */
    virtual void write(LogMessage& message) = 0;

    /** duplicates (clones) this logger. */
    virtual Logger *clone() const = 0;

    /** retrieves the loggers Severity level */
    Severity level() const { return severity_; }

    /** sets the loggers Severity level */
    void setLevel(Severity value) { severity_ = value; }

#if 0
    void write(Severity s, const char* txt)
    {
        LogMessage msg(s, txt);
        this->write(msg);
    }

    template<typename A0, typename... Args>
    void write(Severity s, const char* fmt, A0&& a0, Args&&... args)
    {
        LogMessage msg(s, fmt, a0, args...);
        this->write(msg);
    }
#endif

private:
    Severity severity_;
};

typedef std::shared_ptr<Logger> LoggerPtr;

/** implements a NULL logger (logs nothing).
 *
 * \see logger, FileLogger
 */
class X0_API NullLogger :
    public Logger
{
public:
    NullLogger();
    ~NullLogger();

    virtual void cycle();
    virtual void write(LogMessage& message);
    virtual NullLogger *clone() const;
};

/** implements a file based logger.
 *
 * \see logger, server
 */
class X0_API FileLogger :
    public Logger
{
public:
    FileLogger(const std::string& filename, std::function<time_t()> now);
    FileLogger(int fd, std::function<time_t()> now);
    ~FileLogger();

    virtual void cycle();
    virtual void write(LogMessage& message);
    virtual FileLogger *clone() const;

    int handle() const;

private:
    std::string filename_;
    int fd_;
    std::function<time_t()> now_;
};

class X0_API ConsoleLogger :
    public Logger
{
public:
    ConsoleLogger();

    virtual void cycle();
    virtual void write(LogMessage& message);
    virtual ConsoleLogger* clone() const;
};

/** implements a file based logger.
 *
 * \see logger, server
 */
class X0_API SystemLogger :
    public Logger
{
public:
    SystemLogger();
    ~SystemLogger();

    virtual void cycle();
    virtual void write(LogMessage& message);
    virtual SystemLogger *clone() const;
};

/** implements a file based logger.
 *
 * \see logger, server
 */
class X0_API SystemdLogger :
    public Logger
{
public:
    SystemdLogger();
    ~SystemdLogger();

    virtual void cycle();
    virtual void write(LogMessage& message);
    virtual SystemdLogger *clone() const;
};

// }}}

//@}

} // namespace x0

#endif
