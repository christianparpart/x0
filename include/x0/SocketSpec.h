#ifndef sw_x0_SocketSpec_h
#define sw_x0_SocketSpec_h

#include <x0/Api.h>
#include <x0/IPAddress.h>
#include <string>

namespace x0 {

class X0_API SocketSpec
{
public:
	SocketSpec();

	SocketSpec(const IPAddress& _ipaddr, int _port) :
		address(_ipaddr),
		port(_port),
		backlog(-1),
		valid(true)
	{}

	IPAddress address;
	std::string local;

	int port;
	int backlog;

	bool valid;

	void clear();

	bool isValid() const { return valid; }
	bool isLocal() const { return !local.empty(); }
	bool isInet() const { return local.empty(); }

	std::string str() const;
};

} // namespace x0

#endif
