/* <x0/debug.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */
#ifndef x0_debug_hpp
#define x0_debug_hpp

#include <cstdio>

#ifndef NDEBUG
//#if 0

#define DEBUG(msg...) 			\
	do {						\
		std::printf(msg);		\
		std::printf("\n");		\
	} while (0)

#else
#define DEBUG(msg...) /*;*/
#endif

#endif
