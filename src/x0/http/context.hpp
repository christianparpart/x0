#ifndef sw_x0_http_context_hpp
#define sw_x0_http_context_hpp (1)

#include <x0/api.hpp>

namespace x0 {

enum class context
{
	server		= 0x0001,
	vhost		= 0x0002,
	location	= 0x0004,
	directory	= 0x0008
};


context operator|(context a, context b);
bool operator&(context a, context b);

// {{{ inlines
inline context operator|(context a, context b)
{
	return static_cast<context>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool operator&(context a, context b)
{
	return static_cast<int>(a) & static_cast<int>(b);
}
// }}}

} // namespace x0

#endif
