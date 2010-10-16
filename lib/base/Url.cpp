/* <x0/Url.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Url.h>

namespace x0 {

/** Parses origin server URL.
 *
 * \param spec The URL to parse.
 * \param protocol The parsed protocol value.
 * \param hostname The parsed hostname value (only the hostname).
 * \param port The parsed port number.
 * \param path The parsed path part (may be empty).
 * \param query The parsed query part (may be empty).
 */
bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path, std::string& query)
{
	std::size_t i = spec.find("://");
	if (i == std::string::npos)
		return false;

	protocol = spec.substr(0, i);

	std::size_t k = spec.find("/", i + 3);

	if (k != std::string::npos)
	{
		hostname = spec.substr(i + 3, k - i - 3);
		path = spec.substr(k);

		i = path.find("?");
		if (i != std::string::npos)
		{
			query = path.substr(i + 1);
			path.resize(i);
		}
	}
	else
	{
		hostname = spec.substr(i + 3);
		path.clear();
		query.clear();
	}

	i = hostname.find_last_of(":");
	if (i != std::string::npos)
	{
		port = std::atoi(hostname.c_str() + i + 1);
		hostname.resize(i);
	}
	else
		port = 0;

	if (!port)
	{
		if (protocol == "http")
			port = 80;
		else if (protocol == "https")
			port = 443;
	}

	return true;
}

bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path)
{
	std::string query;
	bool rv = parseUrl(spec, protocol, hostname, port, path, query);
	return rv && query.empty();
}

bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port)
{
	std::string path;
	std::string query;

	bool rv = parseUrl(spec, protocol, hostname, port, path, query);
	return rv && query.empty() && (path.empty() || path == "/");
}

} // namespace x0
