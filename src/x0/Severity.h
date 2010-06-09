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
	static const int error = 0;
	static const int warn = 1;
	static const int info = 2;
	static const int debug = 3;

	int value_;
	Severity(int value) : value_(value) { }
	explicit Severity(const std::string& name);
	operator int() const { return value_; }
	const char *c_str() const;
};

//@}

} // namespace x0

#endif
