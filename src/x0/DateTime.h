/* <x0/DateTime.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_datetime_hpp
#define sw_x0_datetime_hpp 1

#include <x0/Buffer.h>
#include <x0/Api.h>
#include <string>
#include <ctime>

namespace x0 {

//! \addtogroup base
//@{

/** date/time object that understands unix timestamps
 *  as well as HTTP conform dates as used in Date/Last-Modified and other headers.
 */
class X0_API DateTime
{
private:
	time_t unixtime_;
	mutable Buffer http_;
	mutable Buffer htlog_;

	static time_t mktime(const char *v);

public:
	DateTime();
	explicit DateTime(const BufferRef& http_v);
	explicit DateTime(const std::string& http_v);
	explicit DateTime(std::time_t v);

	std::time_t unixtime() const;
	const Buffer& http_str() const;
	const Buffer& htlog_str() const;

	void update();
	void update(std::time_t v);

	bool valid() const;

	static int compare(const DateTime& a, const DateTime& b);
};

X0_API bool operator==(const DateTime& a, const DateTime& b);
X0_API bool operator!=(const DateTime& a, const DateTime& b);
X0_API bool operator<=(const DateTime& a, const DateTime& b);
X0_API bool operator>=(const DateTime& a, const DateTime& b);
X0_API bool operator<(const DateTime& a, const DateTime& b);
X0_API bool operator>(const DateTime& a, const DateTime& b);

// {{{ impl
inline time_t DateTime::mktime(const char *v)
{
	struct tm tm;
	tm.tm_isdst = 0;

	if (strptime(v, "%a, %d %b %Y %H:%M:%S GMT", &tm))
		return ::mktime(&tm) - timezone;

	return 0;
}

inline DateTime::DateTime() :
	unixtime_(std::time(0)), http_(), htlog_()
{
}

/** initializes DateTime object with an HTTP conform input date-time. */
inline DateTime::DateTime(const BufferRef& v) :
	unixtime_(mktime(v.data())), http_(v), htlog_()
{
}

/** initializes DateTime object with an HTTP conform input date-time. */
inline DateTime::DateTime(const std::string& v) :
	unixtime_(mktime(v.c_str())), http_(v), htlog_(v)
{
}

inline DateTime::DateTime(std::time_t v) :
	unixtime_(v), http_(), htlog_()
{
}

inline std::time_t DateTime::unixtime() const
{
	return unixtime_;
}

/** retrieve this dateime object as a HTTP/1.1 conform string.
 * \return HTTP/1.1 conform string value.
 */
inline const Buffer& DateTime::http_str() const
{
	if (http_.empty())
	{
		if (struct tm *tm = gmtime(&unixtime_))
		{
			char buf[256];

			if (strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm) != 0)
			{
				http_ = buf;
			}
		}
	}

	return http_;
}

inline const Buffer& DateTime::htlog_str() const
{
	if (htlog_.empty())
	{
		if (struct tm *tm = localtime(&unixtime_))
		{
			char buf[256];

			if (strftime(buf, sizeof(buf), "%m/%d/%Y:%T %z", tm) != 0)
			{
				htlog_ = buf;
			}
			else
			{
				htlog_ = "-";
			}
		}
		else
		{
			htlog_ = "-";
		}
	}

	return htlog_;
}

inline void DateTime::update()
{
	update(std::time(0));
}

inline void DateTime::update(std::time_t v)
{
	if (unixtime_ != v)
	{
		unixtime_ = v;
		http_.clear();
		htlog_.clear();
	}
}

inline bool DateTime::valid() const
{
	return unixtime_ != 0;
}

inline int DateTime::compare(const DateTime& a, const DateTime& b)
{
	return b.unixtime() - a.unixtime();
}
// }}}

// {{{ compare operators
inline bool operator==(const DateTime& a, const DateTime& b)
{
	return DateTime::compare(a, b) == 0;
}

inline bool operator!=(const DateTime& a, const DateTime& b)
{
	return DateTime::compare(a, b) != 0;
}

inline bool operator<=(const DateTime& a, const DateTime& b)
{
	return DateTime::compare(a, b) <= 0;
}

inline bool operator>=(const DateTime& a, const DateTime& b)
{
	return DateTime::compare(a, b) >= 0;
}

inline bool operator<(const DateTime& a, const DateTime& b)
{
	return DateTime::compare(a, b) < 0;
}

inline bool operator>(const DateTime& a, const DateTime& b)
{
	return DateTime::compare(a, b) > 0;
}
// }}}

//@}

} // namespace x0

#endif
