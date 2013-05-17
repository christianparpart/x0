/* <src/Counter.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */
#include <x0/Counter.h>
#include <x0/JsonWriter.h>

namespace x0 {

Counter::Counter() :
	current_(0),
	max_(0),
	total_(0)
{
}

Counter& Counter::operator++()
{
	increment(1);
	return *this;
}

Counter& Counter::operator+=(size_t n)
{
	increment(n);
	return *this;
}

Counter& Counter::operator--()
{
	decrement(1);
	return *this;
}

Counter& Counter::operator-=(size_t n)
{
	decrement(n);
	return *this;
}

bool Counter::increment(size_t n, size_t expected)
{
	size_t desired = expected + n;

	if (!current_.compare_exchange_weak(expected, desired))
		return false;

	// XXX this might *not always* result into the highest value, but we're fine with it.
	if (desired  > max_.load())
		max_.store(expected + n);

	total_ += n;

	return true;
}

void Counter::increment(size_t n)
{
	current_ += n;

	if (current_ > max_)
		max_.store(current_.load());

	total_ += n;
}

void Counter::decrement(size_t n)
{
	current_ -= n;
}

JsonWriter& operator<<(JsonWriter& json, const Counter& counter)
{
	json.beginObject()
		.name("current")(counter.current())
		.name("max")(counter.max())
		.name("total")(counter.total())
		.endObject();

	return json;
}

} // namespace x0
