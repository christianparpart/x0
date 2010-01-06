/* <x0/datetime.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_datetime_hpp
#define sw_x0_datetime_hpp 1

#include <x0/api.hpp>
#include <string>
#include <ctime>

namespace x0 {

//! \addtogroup base
//@{

/** date/time object that understands unix timestamps
 *  as well as HTTP conform dates as used in Date/Last-Modified and other headers.
 */
class X0_API datetime
{
private:
	time_t unixtime_;
	mutable std::string http_;
	mutable std::string htlog_;

	static time_t mktime(const char *v);

public:
	datetime();
	explicit datetime(const std::string& http_v);
	explicit datetime(std::time_t v);

	std::time_t unixtime() const;
	std::string http_str() const;
	std::string htlog_str() const;

	void update();
	void update(std::time_t v);

	static int compare(const datetime& a, const datetime& b);
};

X0_API bool operator==(const datetime& a, const datetime& b);
X0_API bool operator!=(const datetime& a, const datetime& b);
X0_API bool operator<=(const datetime& a, const datetime& b);
X0_API bool operator>=(const datetime& a, const datetime& b);
X0_API bool operator<(const datetime& a, const datetime& b);
X0_API bool operator>(const datetime& a, const datetime& b);

// {{{ impl
inline time_t datetime::mktime(const char *v)
{
	struct tm tm;
	tm.tm_isdst = 0;

	if (strptime(v, "%a, %d %b %Y %H:%M:%S GMT", &tm))
		return ::mktime(&tm) - timezone;

	return 0;
}

inline datetime::datetime() :
	unixtime_(std::time(0)), http_(), htlog_()
{
}

/** initializes datetime object with an HTTP conform input date-time. */
inline datetime::datetime(const std::string& v) :
	unixtime_(mktime(v.c_str())), http_(v), htlog_(v)
{
}

inline datetime::datetime(std::time_t v) :
	unixtime_(v), http_(), htlog_()
{
}

inline std::time_t datetime::unixtime() const
{
	return unixtime_;
}

/** retrieve this dateime object as a HTTP/1.1 conform string.
 * \return HTTP/1.1 conform string value.
 */
inline std::string datetime::http_str() const
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

inline std::string datetime::htlog_str() const
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

inline void datetime::update()
{
	update(std::time(0));
}

inline void datetime::update(std::time_t v)
{
	if (unixtime_ != v)
	{
		unixtime_ = v;
		http_.clear();
		htlog_.clear();
	}
}

inline int datetime::compare(const datetime& a, const datetime& b)
{
	return b.unixtime() - a.unixtime();
}
// }}}

// {{{ compare operators
inline bool operator==(const datetime& a, const datetime& b)
{
	return datetime::compare(a, b) == 0;
}

inline bool operator!=(const datetime& a, const datetime& b)
{
	return datetime::compare(a, b) != 0;
}

inline bool operator<=(const datetime& a, const datetime& b)
{
	return datetime::compare(a, b) <= 0;
}

inline bool operator>=(const datetime& a, const datetime& b)
{
	return datetime::compare(a, b) >= 0;
}

inline bool operator<(const datetime& a, const datetime& b)
{
	return datetime::compare(a, b) < 0;
}

inline bool operator>(const datetime& a, const datetime& b)
{
	return datetime::compare(a, b) > 0;
}
// }}}

//@}

} // namespace x0

#endif
