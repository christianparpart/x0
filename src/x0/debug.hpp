#ifndef x0_debug_hpp
#define x0_debug_hpp

#include <cstdio>

//#ifndef NDEBUG
#if 0

#define DEBUG(msg...) 			\
	do {						\
		std::printf(msg);		\
		std::printf("\n");		\
	} while (0)

#else
#define DEBUG(msg...) /*;*/
#endif

#endif
