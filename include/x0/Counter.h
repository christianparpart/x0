#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>

namespace x0 {

class X0_API Counter
{
private:
	size_t current_;
	size_t max_;
	size_t total_;

public:
	Counter();
	Counter(const Counter& other) = delete;

	size_t operator()() const { return current_; }

	size_t current() const { return current_; }
	size_t max() const { return max_; }
	size_t total() const { return total_; }

	Counter& operator++();
	Counter& operator--();
};

X0_API Buffer& operator<<(Buffer& output, const Counter& counter);

} // namespace x0
