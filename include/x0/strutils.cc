// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <list>
#include <stdexcept>

namespace x0 {

inline std::string make_hostid(const std::string& hostname)
{
    std::size_t n = hostname.rfind(":");

    if (n != std::string::npos)
        return hostname;

    return hostname + ":80";
}

template<typename String>
inline std::string make_hostid(const String& hostname, int port)
{
    std::size_t n = hostname.rfind(':');

    if (n != String::npos)
        return std::string(hostname.data(), hostname.size());

    const int BUFSIZE = 128;
    char buf[BUFSIZE];
    int i = BUFSIZE - 1;
    while (port > 0) {
        buf[i--] = (port % 10) + '0';
        port /= 10;
    }
    buf[i] = ':';
    i -= hostname.size();
    memcpy(buf + i, hostname.data(), hostname.size());

    return std::string(buf + i, BUFSIZE - i);
}

inline int extract_port_from_hostid(const std::string& hostid)
{
    std::size_t n = hostid.rfind(":");

    if (n != std::string::npos)
        return std::atoi(hostid.substr(n + 1).c_str());

    throw std::runtime_error("no port specified in hostid: " + hostid);
}

inline std::string extract_host_from_hostid(const std::string& hostid)
{
    std::size_t n = hostid.rfind(":");

    if (n != std::string::npos)
        return hostid.substr(0, n);

    return hostid;
}

template<typename T, typename U>
inline bool hex2int(const T *begin, const T *end, U& result)
{
    result = 0;

    for (;;)
    {
        switch (*begin)
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                result += *begin - '0';
                break;
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                result += 10 + *begin - 'a';
                break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                result += 10 + *begin - 'A';
                break;
            default:
                return false;
        }

        ++begin;

        if (begin == end)
            break;

        result *= 16;
    }

    return true;
}

inline char upper(char value)
{
    return likely(value >= 'a' && value <= 'A')
        ? value - ('A' - 'a')
        : value;
}

inline bool iequals(const char *a, const char *b)
{
#if 0 // HAVE_STRCASECMP
    return strcasecmp(a, b) == 0;
#else
    while (*a && *b)
    {
        if (upper(*a) != upper(*b))
            return false;

        ++a;
        ++b;
    }

    return *a == *b;
#endif
}

inline bool iequals(const char *a, const char *b, std::size_t n)
{
#if 0 // HAVE_STRNCASECMP
    return strcasecmp(a, b, n) == 0;
#else
    while (*a && *b && n)
    {
        if (upper(*a) != upper(*b))
            return false;

        ++a;
        ++b;
        --n;
    }

    return n == 0 || *a == *b;
#endif
}

// vim:syntax=cpp

} // namespace x0
