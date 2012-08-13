/* <Counter.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>

#include <atomic>

namespace x0 {

class X0_API Counter
{
private:
	std::atomic<size_t> current_;
	std::atomic<size_t> max_;
	std::atomic<size_t> total_;

public:
	Counter();
	Counter(const Counter& other) = delete;
	Counter& operator=(const Counter& other) = delete;

	size_t operator()() const { return current_.load(); }

	size_t current() const { return current_.load(); }
	size_t max() const { return max_.load(); }
	size_t total() const { return total_.load(); }

	Counter& operator++();
	Counter& operator--();
};

X0_API Buffer& operator<<(Buffer& output, const Counter& counter);

} // namespace x0
