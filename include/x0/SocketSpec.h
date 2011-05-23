#ifndef sw_x0_SocketSpec_h
#define sw_x0_SocketSpec_h

#include <x0/Api.h>
#include <x0/IPAddress.h>
#include <string>

namespace x0 {

struct X0_API SocketSpec
{
	IPAddress address;
	std::string local;

	int port;
	int backlog;

	bool valid;

	SocketSpec();

	void clear();

	bool isValid() const { return valid; }
	bool isLocal() const { return !local.empty(); }
	bool isInet() const { return local.empty(); }

	std::string str() const;
};

} // namespace x0

#endif
