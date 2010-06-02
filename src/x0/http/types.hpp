#ifndef sw_x0_http_types_hpp
#define sw_x0_http_types_hpp (1)

#include <x0/types.hpp>
#include <x0/api.hpp>

namespace x0 {

class settings_value;
class scope;

typedef std::function<bool(const settings_value&, scope&)> cvar_handler;

} // namespace x0

#endif
