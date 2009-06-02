/* <x0/logger.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_errorlog_h
#define sw_x0_errorlog_h

#include <x0/types.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

namespace x0 {

/**
 * \ingroup core
 * \brief logging facility.
 *
 * \see filelogger, server
 */
class logger :
	public boost::noncopyable
{
public:
	logger();
	virtual ~logger();

	/** reallocates resources used by this logger. */
	virtual void cycle() = 0;

	/** writes a message into the logger. */
	virtual void write(severity s, const std::string& message) = 0;

	/** duplicates (clones) this logger. */
	virtual logger *clone() const = 0;
};

typedef boost::shared_ptr<logger> logger_ptr;

/**
 * \ingroup core
 * \brief implements a NULL logger (logs nothing).
 * \see logger, filelogger
 */
class nulllogger :
	public logger
{
public:
	nulllogger();
	~nulllogger();

	virtual void cycle();
	virtual void write(severity s, const std::string& message);
	virtual nulllogger *clone() const;
};

/**
 * \ingroup core
 * \brief implements a file based logger.
 *
 * \see logger, server
 */
class filelogger :
	public logger
{
public:
	explicit filelogger(const std::string& filename);
	~filelogger();

	virtual void cycle();
	virtual void write(severity s, const std::string& message);
	virtual filelogger *clone() const;

private:
	std::string filename_;
	int fd_;
};

} // namespace x0

#endif
