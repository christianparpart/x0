/* <Severity.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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
		debug3 = 0,
		debug2 = 1,
		debug1 = 2,
		info = 3,
		notice = 4,
		warning = 5,
		error = 6,
		crit = 7,
		alert = 8,
		emerg = 9,

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

	bool set(const char* value);
};

//@}

} // namespace x0

#endif
