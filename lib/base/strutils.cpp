/* <x0/strutils.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/strutils.h>
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
