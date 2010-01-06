/* <x0/severity.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_severity_hpp
#define x0_severity_hpp (1)

#include <x0/types.hpp>
#include <x0/api.hpp>
#include <string>

namespace x0 {

//! \addtogroup base
//@{

/**
 * \brief named enum `severity`, used by logging facility
 * \see logger
 */
struct X0_API severity {
	static const int emergency = 0;
	static const int alert = 1;
	static const int critical = 2;
	static const int error = 3;
	static const int warn = 4;
	static const int notice = 5;
	static const int info = 6;
	static const int debug = 7;

	int value_;
	severity(int value) : value_(value) { }
	explicit severity(const std::string& name);
	operator int() const { return value_; }
	const char *c_str() const;
};

//@}

} // namespace x0

#endif
