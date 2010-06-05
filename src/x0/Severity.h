/* <x0/Severity.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
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
	static const int emergency = 0;
	static const int alert = 1;
	static const int critical = 2;
	static const int error = 3;
	static const int warn = 4;
	static const int notice = 5;
	static const int info = 6;
	static const int debug = 7;

	int value_;
	Severity(int value) : value_(value) { }
	explicit Severity(const std::string& name);
	operator int() const { return value_; }
	const char *c_str() const;
};

//@}

} // namespace x0

#endif
