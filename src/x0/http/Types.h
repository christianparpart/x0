#ifndef sw_x0_http_types_hpp
#define sw_x0_http_types_hpp (1)

#include <x0/Types.h>
#include <x0/Api.h>

namespace x0 {

class SettingsValue;
class Scope;

typedef std::function<bool(const SettingsValue&, Scope&)> cvar_handler;

} // namespace x0

#endif
