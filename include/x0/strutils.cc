/* <strutils.cc>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <list>

namespace x0 {

template<typename T, typename U>
inline std::vector<T> split(const std::basic_string<U>& input, const std::basic_string<U>& sep)
{
	std::vector<T> result;
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer tk(input, boost::char_separator<U>(sep.c_str()));

	for (tokenizer::iterator i = tk.begin(), e = tk.end(); i != e; ++i)
	{
		result.push_back(boost::lexical_cast<T>(*i));
	}

	return result;
}

template<typename T, typename U>
inline std::vector<T> split(const std::basic_string<U>& list, const U *sep)
{
	return split<T, U>(list, std::basic_string<U>(sep));
}

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

#if 0
	return hostname + ":" + boost::lexical_cast<std::string>(port);
#else
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
#endif
}

inline int extract_port_from_hostid(const std::string& hostid)
{
	std::size_t n = hostid.rfind(":");

	if (n != std::string::npos)
		return boost::lexical_cast<int>(hostid.substr(n + 1));

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

// vim:syntax=cpp

} // namespace x0
