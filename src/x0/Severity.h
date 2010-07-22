/* <Severity.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_Severity_h
#define x0_Severity_h (1)

#include <x0/Types.h>
#include <x0/Api.h>

#include <string>

namespace x0 {

//! \addtogroup base
//@{

/**
 * \brief named enum `Severity`, used by logging facility
 * \see logger
 */
struct X0_API Severity {
	static const int error = -3;
	static const int warn = -2;
	static const int info = -1;
	static const int debug = 1; // and everything from [1..10] (or any positive number) *could* be treated as debug verbosity

	int value_;

	Severity(int value) : value_(value) { }
	explicit Severity(const std::string& name);
	operator int() const { return value_; }
	const char *c_str() const;

	bool isError() const { return value_ == error; }
	bool isWarning() const { return value_ == warn; }
	bool isInfo() const { return value_ == info; }
	bool isDebug() const { return value_ > 1; }

	int debugLevel() const { return value_ < 0 ? 0 : value_; }
};

//@}

} // namespace x0

#endif
