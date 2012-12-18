/* <Severity.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
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
	enum {
		error = 0,
		warning = 1,
		notice = 2,
		info = 3,
		debug1 = 4,
		debug2 = 5,
		debug3 = 6,
		debug4 = 7,
		debug5 = 8,
		debug6 = 9,

		warn = warning,
		debug = debug1
	};

	int value_;

	Severity(int value) : value_(value) { }
	explicit Severity(const std::string& name);
	operator int() const { return value_; }
	const char *c_str() const;

	bool isError() const { return value_ == error; }
	bool isWarning() const { return value_ == warn; }
	bool isInfo() const { return value_ == info; }
	bool isDebug() const { return value_ >= debug; }

	int debugLevel() const { return isDebug() ? 1 + value_ - debug1 : 0; }
};

//@}

} // namespace x0

#endif
