/* <x0/strutils.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
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

bool isdir(const std::string& filename)
{
	struct stat st;
	return stat(filename.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

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
			if (::read(fd, buf, st.st_size) != -1)
			{
				std::string str(buf, buf + st.st_size);
				delete[] buf;
				return str;
			}
		}
	}
	return std::string();
#endif
}

std::string trim(const std::string value)
{
	std::size_t left = 0;
	while (std::isspace(value[left]))
		++left;

	std::size_t right = value.size() - 1;
	while (std::isspace(value[right]))
		--right;

	return std::string(value.data() + left, value.data() + right + 1);
}

} // namespace x0
