/* <x0/strutils.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_strutils_hpp
#define x0_strutils_hpp (1)

#include <x0/types.hpp>
#include <x0/api.hpp>
#include <string>
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
	void build(const char *s, const T&& value, const Args&&... args)
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
std::string read_file(const std::string& filename);

/**
 * trims leading and trailing spaces off the value.
 */
std::string trim(const std::string& value);

/**
 * splits a string into pieces
 */
template<typename T, typename U>
std::vector<T> split(const std::basic_string<U>& list, const std::basic_string<U>& sep);

/**
 * splits a string into pieces
 */
template<typename T, typename U>
std::vector<T> split(const std::basic_string<U>& list, const U *sep);

/**
 * generates an HTTP/1.1 conform date.
 *
 * \param ts the unix timestamp to generate the HTTP date for.
 * \return string value containing HTTP/1.1 conform date equivalent to \p ts.
 *
 * \note Date format is equivalent to RFC 822.
 */
std::string http_date(std::time_t ts);

std::string make_hostid(const std::string& hostname);
std::string make_hostid(const std::string& hostname, int port);
int extract_port_from_hostid(const std::string& hostid);
std::string extract_host_from_hostid(const std::string& hostid);

//@}

} // namespace x0

#include <x0/strutils.tcc>

#endif
