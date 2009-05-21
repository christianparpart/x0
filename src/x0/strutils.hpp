/* <x0/strutils.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_strutils_hpp
#define x0_strutils_hpp (1)

#include <x0/types.hpp>
#include <string>

namespace x0 {

/** \addtogroup common */
/*@{*/

/**
 * retrieves contents of given file.
 */
std::string read_file(const std::string& filename);

/**
 * trims leading and trailing spaces off the value.
 */
std::string trim(const std::string value);

/*@}*/

} // namespace x0

#endif
