/* <x0/TimeSpan.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2011 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_TimeSpan_h
#define sw_x0_TimeSpan_h

#include <x0/Buffer.h>
#include <x0/Api.h>
#include <cstdio>
#include <ev.h>

namespace x0 {

class X0_API TimeSpan
{
private:
	ev_tstamp value_;

public:
	TimeSpan() : value_(0) {}
	TimeSpan(ev_tstamp v) : value_(v) {}
	TimeSpan(std::size_t v) : value_(v) {}
	TimeSpan(const TimeSpan& v) : value_(v.value_) {}

	ev_tstamp value() const { return value_; }
	ev_tstamp operator()() const { return value_; }

	int days() const { return ((int)value_ / ticksPerDay()); }
	int hours() const { return ((int)value_ / ticksPerHour()) - 24 * ((int)value_ / ticksPerDay()); }
	int minutes() const { return ((int)value_ / 60) - 60 * ((int)value_ / 3600); }
	int seconds() const { return static_cast<int>(value_) % 60; }
	int milliseconds() const;

	static inline int ticksPerDay() { return 86400; }
	static inline int ticksPerHour() { return 3600; }
	static inline int ticksPerMinute() { return 60; }
	static inline int ticksPerSecond() { return 1; }

	static TimeSpan fromDays(std::size_t v) { return TimeSpan(ticksPerDay() * v); }
	static TimeSpan fromHours(std::size_t v) { return TimeSpan(ticksPerHour() * v); }
	static TimeSpan fromMinutes(std::size_t v) { return TimeSpan(ticksPerMinute() * v); }
	static TimeSpan fromSeconds(std::size_t v) { return TimeSpan(ticksPerSecond() * v); }
	static TimeSpan fromMilliseconds(std::size_t v) { return TimeSpan(ev_tstamp(v / 1000 + (unsigned(v) % 1000) / 1000.0)); }

	std::size_t totalMilliseconds() const { return value_ * 1000; }

	bool operator!() const { return value_ == 0; }
	operator bool () const { return value_ != 0; }

	std::string str() const;

	static const TimeSpan Zero;
};

inline bool operator==(const TimeSpan& a, const TimeSpan& b)
{
	return a() == b();
}

inline bool operator!=(const TimeSpan& a, const TimeSpan& b)
{
	return a() != b();
}

inline bool operator<(const TimeSpan& a, const TimeSpan& b)
{
	return a() < b();
}

inline bool operator<=(const TimeSpan& a, const TimeSpan& b)
{
	return a() <= b();
}

inline bool operator>(const TimeSpan& a, const TimeSpan& b)
{
	return a() > b();
}

inline bool operator>=(const TimeSpan& a, const TimeSpan& b)
{
	return a() >= b();
}

inline TimeSpan operator+(const TimeSpan& a, const TimeSpan& b)
{
	return TimeSpan(a() + b());
}

inline TimeSpan operator-(const TimeSpan& a, const TimeSpan& b)
{
	return TimeSpan(a() - b());
}

inline Buffer& operator<<(Buffer& buf, const TimeSpan& ts)
{
	char tmp[64];
	int n = snprintf(tmp, sizeof(tmp), "%d.%02d:%02d:%02d", ts.days(), ts.hours(), ts.minutes(), ts.seconds());
	buf.push_back(tmp, n);
	return buf;
}

} // namespace x0

#endif
