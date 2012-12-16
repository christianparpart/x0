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

	Type type() const { return type_; }
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


X0_API bool operator==(const x0::SocketSpec& a, const x0::SocketSpec& b);
X0_API bool operator!=(const x0::SocketSpec& a, const x0::SocketSpec& b);

inline X0_API bool operator==(const x0::SocketSpec& a, const x0::SocketSpec& b)
{
	if (a.type() != b.type())
		return false;

	switch (a.type()) {
		case x0::SocketSpec::Local:
			return a.port() == b.port() && a.ipaddr() == b.ipaddr();
		case x0::SocketSpec::Inet:
			return a.local() == b.local();
		default:
			return false;
	}
}

inline X0_API bool operator!=(const x0::SocketSpec& a, const x0::SocketSpec& b)
{
	return !(a == b);
}
} // namespace x0

namespace std
{
	template <>
	struct hash<x0::SocketSpec> :
		public unary_function<x0::SocketSpec, size_t>
	{
		size_t operator()(const x0::SocketSpec& v) const
		{
			switch (v.type()) {
				case x0::SocketSpec::Inet:
					return hash<size_t>()(v.type()) ^ hash<x0::IPAddress>()(v.ipaddr());
				case x0::SocketSpec::Local:
					return hash<size_t>()(v.type()) ^ hash<string>()(v.local());
				default:
					return 0;
			}
		}
	};
}

#endif
