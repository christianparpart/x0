/* <Counter.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>

#include <cstdint>
#include <atomic>

namespace x0 {

class JsonWriter;

class X0_API Counter
{
public:
	typedef size_t value_type;

private:
	std::atomic<value_type> current_;
	std::atomic<value_type> max_;
	std::atomic<value_type> total_;

public:
	Counter();
	Counter(const Counter& other) = delete;
	Counter& operator=(const Counter& other) = delete;

	value_type operator()() const { return current_.load(); }

	value_type current() const { return current_.load(); }
	value_type max() const { return max_.load(); }
	value_type total() const { return total_.load(); }

	Counter& operator++();
	Counter& operator+=(size_t n);
	Counter& operator--();
	Counter& operator-=(size_t n);

	bool compare_incr(size_t expected, size_t n);
};

inline bool Counter::compare_incr(size_t expected, size_t n)
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

X0_API JsonWriter& operator<<(JsonWriter& json, const Counter& counter);

} // namespace x0
