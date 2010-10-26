/* <x0/AtomicInt.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_AtomicInt_h
#define sw_x0_AtomicInt_h

#include <ext/atomicity.h> // gcc extension; TODO: support other compilers/platforms, too.

namespace x0 {

class AtomicInt
{
public:
	typedef int value_type;

private:
	value_type value_;

public:
	AtomicInt() : value_() {}
	AtomicInt(value_type val) : value_(val) {}

	operator value_type() const { return value_; }

	AtomicInt& operator+=(value_type val)
	{
		__gnu_cxx::__atomic_add(&value_, val);
		return *this;
	}

	AtomicInt& operator-=(value_type val)
	{
		return operator+=(-val);
	}

	AtomicInt& operator++()
	{
		return operator+=(1);
	}

	AtomicInt& operator--()
	{
		return operator+=(-1);
	}
};

} // namespace x0

#endif
