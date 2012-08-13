/* <SocketSpec.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_SocketSpec_h
#define sw_x0_SocketSpec_h

#include <x0/Api.h>
#include <x0/IPAddress.h>
#include <string>

namespace x0 {

class X0_API SocketSpec
{

public:
	enum Type {
		Unknown,
		Local,
		Inet,
	};

	SocketSpec();
	SocketSpec(const SocketSpec& ss);
	SocketSpec(const IPAddress& ipaddr, int port, int backlog = -1) :
		type_(Inet),
		ipaddr_(ipaddr),
		port_(port),
		backlog_(backlog)
	{}

	static SocketSpec fromString(const std::string& value);
	static SocketSpec fromLocal(const std::string& path, int backlog = -1);
	static SocketSpec fromInet(const IPAddress& ipaddr, int port, int backlog = -1);

	void clear();

	bool isValid() const { return type_ != Unknown; }
	bool isLocal() const { return type_ == Local; }
	bool isInet() const { return type_ == Inet; }

	const IPAddress& ipaddr() const { return ipaddr_; }
	int port() const { return port_; }
	const std::string& local() const { return local_; }
	int backlog() const { return backlog_; }

	void setPort(int value);
	void setBacklog(int value);

	std::string str() const;

private:
	Type type_;
	IPAddress ipaddr_;
	std::string local_;
	int port_;
	int backlog_;
};

} // namespace x0

#endif
