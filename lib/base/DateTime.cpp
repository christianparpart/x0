/* <src/DateTime.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/DateTime.h>
#include <pthread.h>

namespace x0 {

DateTime::DateTime() :
	value_(std::time(0)), http_(), htlog_()
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

/** initializes DateTime object with an HTTP conform input date-time. */
DateTime::DateTime(const BufferRef& v) :
	value_(mktime(v.data())), http_(v), htlog_()
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

/** initializes DateTime object with an HTTP conform input date-time. */
DateTime::DateTime(const std::string& v) :
	value_(mktime(v.c_str())), http_(v), htlog_(v)
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

DateTime::DateTime(ev::tstamp v) :
	value_(v), http_(), htlog_()
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

DateTime::~DateTime()
{
	pthread_spin_destroy(&lock_);
}

/** retrieve this dateime object as a HTTP/1.1 conform string.
 * \return HTTP/1.1 conform string value.
 */
const Buffer& DateTime::http_str() const
{
	pthread_spin_lock(&lock_);
	if (http_.empty())
	{
		std::time_t ts = unixtime();
		if (struct tm *tm = gmtime(&ts))
		{
			char buf[256];

			if (strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm) != 0)
			{
				http_ = buf;
			}
		}
	}
	pthread_spin_unlock(&lock_);

	return http_;
}

const Buffer& DateTime::htlog_str() const
{
	pthread_spin_lock(&lock_);
	if (htlog_.empty())
	{
		std::time_t ts = unixtime();
		if (struct tm *tm = localtime(&ts))
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
	pthread_spin_unlock(&lock_);

	return htlog_;
}

} // namespace x0
