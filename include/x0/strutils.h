// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_strutils_hpp
#define x0_strutils_hpp (1)

#include <x0/Api.h>
#include <x0/Types.h>
#include <x0/Buffer.h>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>

#if !defined(BOOST_HAS_VARIADIC_TMPL)
# include <cstdarg>
#endif

namespace x0 {

//! \addtogroup base
//@{

// {{{ fstringbuilder

/**
 * a safe printf string builder.
 */
class fstringbuilder
{
public:
    /**
     * formats given string.
     */
#ifdef BOOST_HAS_VARIADIC_TMPL
    template<typename... Args>
    static std::string format(const char *s, Args&&... args)
    {
        fstringbuilder fsb;
        fsb.build(s, args...);
        return fsb.sstr_.str();
    }
#else
    static std::string format(const char *s, ...)
    {
        va_list va;
        char buf[1024];

        va_start(va, s);
        vsnprintf(buf, sizeof(buf), s, va);
        va_end(va);

        return buf;
    }
#endif

#ifdef BOOST_HAS_VARIADIC_TMPL
private:
    void build(const char *s)
    {
        if (*s == '%' && *++s != '%')
            throw std::runtime_error("invalid format string: missing arguments");

        sstr_ << *s;
    }

    template<typename T, typename... Args>
    void build(const char *s, T&& value, Args&&... args)
    {
        while (*s)
        {
            if (*s == '%' && *++s != '%')
            {
                sstr_ << value;
                return build(++s, args...);
            }
            sstr_ << *s++;
        }
        throw std::runtime_error("extra arguments provided to printf");
    }

private:
    std::stringstream sstr_;
#endif
};
// }}}

/**
 * retrieves contents of given file.
 */
X0_API Buffer readFile(const std::string& filename);


/**
 * trims leading and trailing spaces off the value.
 */
X0_API std::string trim(const std::string& value);

/**
 * splits a string into pieces
 */
template<typename T, typename U>
X0_API std::vector<T> split(const std::basic_string<U>& list, const std::basic_string<U>& sep);

/**
 * splits a string into pieces
 */
template<typename T, typename U>
X0_API std::vector<T> split(const std::basic_string<U>& list, const U *sep);

template<typename T, typename U>
X0_API inline bool hex2int(const T *begin, const T *end, U& result);

/*! compares two strings for case insensitive equality. */
X0_API bool iequals(const char *a, const char *b);

/*! compares the first n bytes of two strings for case insensitive equality. */
X0_API bool iequals(const char *a, const char *b, std::size_t n);

//@}

//! \addtogroup base
//@{

X0_API std::string make_hostid(const std::string& hostname);
template<typename String> X0_API std::string make_hostid(const String& hostname, int port);
X0_API int extract_port_from_hostid(const std::string& hostid);
X0_API std::string extract_host_from_hostid(const std::string& hostid);

//@}

} // namespace x0

#include <x0/strutils.cc>

#endif
