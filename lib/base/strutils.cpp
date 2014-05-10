/* <x0/strutils.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/strutils.h>
#include <x0/Buffer.h>
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

Buffer readFile(const std::string& filename)
{
    Buffer result;

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0)
        return result;

    for (;;) {
        char buf[4096];
        ssize_t nread = ::read(fd, buf, sizeof(buf));
        if (nread <= 0)
            break;

        result.push_back(buf, nread);
    }

    ::close(fd);

    return result;
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

template<> std::string lexical_cast<std::string, unsigned int>(const unsigned int& value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%u", value);
    return buf;
}

template<> X0_API std::string lexical_cast<std::string, unsigned long>(const unsigned long& value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%lu", value);
    return buf;
}

template<> X0_API std::string lexical_cast<std::string, unsigned long long>(const unsigned long long& value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%llu", value);
    return buf;
}

} // namespace x0
