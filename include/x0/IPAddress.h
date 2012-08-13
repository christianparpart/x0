/* <IPAddress.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_IPAddress_h
#define x0_IPAddress_h

#include <x0/Api.h>

#include <cstdint>
#include <string>
#include <string.h>     // memset()
#include <netinet/in.h> // in_addr, in6_addr
#include <arpa/inet.h>  // ntohl(), htonl()
#include <stdio.h>
#include <stdlib.h>

namespace x0 {

class X0_API IPAddress
{
public:
	static const int V4 = AF_INET;
	static const int V6 = AF_INET6;

private:
	int family_;
	uint8_t buf_[sizeof(struct in6_addr)];

public:
	IPAddress();
	explicit IPAddress(const std::string& text, int family = 0);

	IPAddress& operator=(const std::string& value);
	IPAddress& operator=(const IPAddress& value);

	bool set(const std::string& text, int family);

	int family() const;
	const void *data() const;
	size_t size() const;
	std::string str() const;

	friend bool operator==(const IPAddress& a, const IPAddress& b);
	friend bool operator!=(const IPAddress& a, const IPAddress& b);
};

// {{{ impl
inline IPAddress::IPAddress()
{
	family_ = 0;
	memset(buf_, 0, sizeof(buf_));
}

inline IPAddress::IPAddress(const std::string& text, int family)
{
	if (family != 0) {
		set(text, family);
	} else if (text.find(':') != std::string::npos) {
		set(text, AF_INET6);
	} else {
		set(text, AF_INET);
	}
}

inline IPAddress& IPAddress::operator=(const std::string& text)
{
	if (text.find(':') != std::string::npos) {
		set(text, AF_INET6);
	} else {
		set(text, AF_INET);
	}
	return *this;
}

inline IPAddress& IPAddress::operator=(const IPAddress& v)
{
	family_ = v.family_;
	memcpy(buf_, v.buf_, v.size());

	return *this;
}

inline bool IPAddress::set(const std::string& text, int family)
{
	family_ = family;
	int rv = inet_pton(family, text.c_str(), buf_);
	if (rv <= 0) {
		if (rv < 0)
			perror("inet_pton");
		else
			fprintf(stderr, "IP address Not in presentation format: %s\n", text.c_str());

		return false;
	}
	return true;
}

inline int IPAddress::family() const
{
	return family_;
}

inline const void *IPAddress::data() const
{
	return buf_;
}

inline size_t IPAddress::size() const
{
	return family_ == V4
		? sizeof(in_addr)
		: sizeof(in6_addr);
}

inline std::string IPAddress::str() const
{
	char result[INET6_ADDRSTRLEN];
	inet_ntop(family_, &buf_, result, sizeof(result));

	return result;
}

inline bool operator==(const IPAddress& a, const IPAddress& b)
{
	if (&a == &b)
		return true;

	if (a.family() != b.family())
		return false;

	switch (a.family()) {
		case AF_INET:
		case AF_INET6:
			return memcmp(a.data(), b.data(), a.size()) == 0;
		default:
			return false;
	}

	return false;
}

inline bool operator!=(const IPAddress& a, const IPAddress& b)
{
	return !(a == b);
}
// }}}

} // namespace x0

#endif
