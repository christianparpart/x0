/* <x0/strutils.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/strutils.hpp>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cctype>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

std::string read_file(const std::string& filename)
{
#if 0
	// XXX gcc 4.4.0: ld error, can't resolv fstream.constructor or fstream.open - wtf?
	std::ifstream ifs(filename);
	std::stringstream sstr;
	sstr << ifs.rdbuf();
	std::string str(sstr.str());
	return str;
#else
	struct stat st;
	if (stat(filename.c_str(), &st) != -1)
	{
		int fd = open(filename.c_str(), O_RDONLY);
		if (fd != -1)
		{
			char *buf = new char[st.st_size + 1];
			ssize_t nread = ::read(fd, buf, st.st_size);
			if (nread != -1)
			{
				buf[nread] = '\0';
				std::string str(buf, 0, nread);
				delete[] buf;
				::close(fd);
				return str;
			}
			delete[] buf;
			::close(fd);
		}
		throw std::runtime_error(fstringbuilder::format("cannot open file: %s (%s)", filename.c_str(), strerror(errno)));
	}
	else
	{
		throw std::runtime_error(fstringbuilder::format("cannot open file: %s (%s)", filename.c_str(), strerror(errno)));
	}
#endif
}

std::string trim(const std::string& value)
{
	std::size_t left = 0;
	while (std::isspace(value[left]))
		++left;

	std::size_t right = value.size() - 1;
	while (std::isspace(value[right]))
		--right;

	return value.substr(left, 1 + right - left);
}

/** Parses origin server URL.
 *
 * \param spec The URL to parse.
 * \param protocol The parsed protocol value.
 * \param hostname The parsed hostname value (only the hostname).
 * \param port The parsed port number.
 * \param path The parsed path part (may be empty).
 * \param query The parsed query part (may be empty).
 */
bool parse_url(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path, std::string& query)
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

bool parse_url(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path)
{
	std::string query;
	bool rv = parse_url(spec, protocol, hostname, port, path, query);
	return rv && query.empty();
}

bool parse_url(const std::string& spec, std::string& protocol, std::string& hostname, int& port)
{
	std::string path;
	std::string query;

	bool rv = parse_url(spec, protocol, hostname, port, path, query);
	return rv && query.empty() && (path.empty() || path == "/");
}

} // namespace x0
