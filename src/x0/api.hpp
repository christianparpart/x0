#ifndef sw_x0_api_hpp
#define sw_x0_api_hpp (1)

#include <x0/platform.hpp>

// x0 exports
#if defined(BUILD_X0)
#	define X0_API X0_EXPORT
#else
#	define X0_API X0_IMPORT
#endif

#endif
