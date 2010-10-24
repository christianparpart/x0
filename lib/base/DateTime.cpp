#include <x0/DateTime.h>
#include <pthread.h>

namespace x0 {

DateTime::DateTime() :
	unixtime_(std::time(0)), http_(), htlog_()
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

/** initializes DateTime object with an HTTP conform input date-time. */
DateTime::DateTime(const BufferRef& v) :
	unixtime_(mktime(v.data())), http_(v), htlog_()
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

/** initializes DateTime object with an HTTP conform input date-time. */
DateTime::DateTime(const std::string& v) :
	unixtime_(mktime(v.c_str())), http_(v), htlog_(v)
{
	pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

DateTime::DateTime(std::time_t v) :
	unixtime_(v), http_(), htlog_()
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
		if (struct tm *tm = gmtime(&unixtime_))
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
	pthread_spin_unlock(&lock_);

	return htlog_;
}

} // namespace x0
