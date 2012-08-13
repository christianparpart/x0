/* <src/Counter.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */
#include <x0/Counter.h>

namespace x0 {

Counter::Counter() :
	current_(0),
	max_(0),
	total_(0)
{
}

Counter& Counter::operator++()
{
	++current_;

	if (current_ > max_)
		max_.store(current_.load());

	++total_;

	return *this;
}

Counter& Counter::operator--()
{
	--current_;

	return *this;
}

Buffer& operator<<(Buffer& output, const Counter& counter)
{
	output
		<< "{"
		<< "\"current\": " << counter.current() << ", "
		<< "\"max\": " << counter.max() << ", "
		<< "\"total\": " << counter.total()
		<< "}";

	return output;
}

} // namespace x0
