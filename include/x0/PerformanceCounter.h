/* <PerformanceCounter.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_PerformanceCounter_h
#define sw_x0_PerformanceCounter_h

#include <x0/Api.h>

namespace x0 {

template<const unsigned PERIOD = 60, typename T = double>
class X0_API PerformanceCounter
{
public:
	typedef T value_type;

private:
	unsigned counter_[PERIOD];
	time_t last_;

public:
	PerformanceCounter();

	void clear();
	void touch(time_t now, unsigned value = 1);

	unsigned current() const;
	value_type average(unsigned n = PERIOD) const;

	unsigned operator[](size_t i) const;
};

// {{{ inlines
template<const unsigned PERIOD, typename T>
inline PerformanceCounter<PERIOD, T>::PerformanceCounter()
{
	clear();
}

template<const unsigned PERIOD, typename T>
inline void PerformanceCounter<PERIOD, T>::clear()
{
	for (auto& i: counter_)
		i = 0;

	last_ = 1;
}

template<const unsigned PERIOD, typename T>
inline void PerformanceCounter<PERIOD, T>::touch(time_t now, unsigned value)
{
	const size_t i = now % PERIOD;   // offset into counter, for now
	const size_t diff = now - last_; // seconds difference between now and last

	if (diff == 0) {
		// same second
		counter_[i] += value;
	} else {
		if (diff < PERIOD) {
			size_t i0 = last_ % PERIOD; // last offset into counter

			if (i0 < i) {
				for (size_t k = i0 + 1; k < i; ++k) {
					counter_[k] = 0;
				}
			} else {
				while (++i0 < PERIOD) {
					counter_[i0] = 0;
				}

				for (i0 = 0; i0 < i; ++i0) {
					counter_[i0] = 0;
				}
			}
		} else {
			for (auto& k: counter_) {
				k = 0;
			}
		}

		counter_[i] = value;
		last_ = now;
	}
}

template<const unsigned PERIOD, typename T>
inline unsigned PerformanceCounter<PERIOD, T>::current() const
{
	size_t prev = (last_ - 1) % PERIOD;
	if (prev == 0)
		prev = PERIOD - 1;

	return counter_[prev];
}

template<const unsigned PERIOD, typename T>
inline T PerformanceCounter<PERIOD, T>::average(unsigned n) const
{
	value_type result = 0;
	unsigned k = n - 1;

	size_t prev = (last_ - 1) % PERIOD;
	if (prev == 0)
		prev = PERIOD - 1;

	for (size_t i = prev; i > 0 && k > 0; --i, --k)
		result += counter_[i];

	for (size_t i = PERIOD - 1; k > 0; --k)
		result += counter_[i];

	result /= n;

	return result;
}

template<const unsigned PERIOD, typename T>
unsigned PerformanceCounter<PERIOD, T>::operator[](size_t i) const
{
	return counter_[PERIOD - i];
}
// }}}

} // namespace x0

#endif
