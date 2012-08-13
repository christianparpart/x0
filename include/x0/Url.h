/* <x0/Url.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#if !defined(sw_x0_url_hpp)
#define sw_x0_url_hpp (1)

#include <x0/Api.h>
#include <string>

namespace x0 {

class X0_API Url
{
public:
	static Url parse(const std::string& url);

	Url();
	Url(const Url& url);
	Url(Url&& url);
	~Url();

	Url& operator=(const Url& url);

	const std::string& protocol() const { return protocol_; }
	const std::string& username() const { return username_; }
	const std::string& password() const { return password_; }
	const std::string& hostname() const { return hostname_; }
	int port() const { return port_; }
	const std::string& path() const { return path_; }
	const std::string& query() const { return query_; }
	const std::string& fragment() const { return fragment_; }

private:
	std::string protocol_;
	std::string username_;
	std::string password_;
	std::string hostname_;
	int port_;
	std::string path_;
	std::string query_;
	std::string fragment_;
};

X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path, std::string& query);
X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path);
X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port);

} // namespace x0

#endif
