#pragma once

#include <x0/Api.h>
#include <string>

namespace x0 {

class X0_API HttpAuthBackend
{
public:
	virtual ~HttpAuthBackend() {}

	virtual bool authenticate(const std::string& username, const std::string& passwd) = 0;
};

} // namespace x0
