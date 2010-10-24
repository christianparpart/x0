/* <x0/DateTime.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_datetime_hpp
#define sw_x0_datetime_hpp 1

#include <x0/Buffer.h>
#include <x0/Api.h>
#include <string>
#include <ctime>
#include <pthread.h>

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
	mutable pthread_spinlock_t lock_;

	static time_t mktime(const char *v);

public:
	DateTime();
	explicit DateTime(const BufferRef& http_v);
	explicit DateTime(const std::string& http_v);
	explicit DateTime(std::time_t v);
	~DateTime();

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

inline std::time_t DateTime::unixtime() const
{
	return unixtime_;
}

inline void DateTime::update()
{
	update(std::time(0));
}

inline void DateTime::update(std::time_t v)
{
	if (unixtime_ != v)
	{
		pthread_spin_lock(&lock_);
		unixtime_ = v;
		http_.clear();
		htlog_.clear();
		pthread_spin_unlock(&lock_);
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
