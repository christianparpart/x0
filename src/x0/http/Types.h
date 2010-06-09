#ifndef sw_x0_http_types_hpp
#define sw_x0_http_types_hpp (1)

#include <x0/Types.h>
#include <x0/Api.h>

#include <system_error>

namespace x0 {

class SettingsValue;
class Scope;

typedef std::function<std::error_code(const SettingsValue&, Scope&)> cvar_handler;

} // namespace x0

#endif
