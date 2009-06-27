/* <x0/strutils.tcc>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
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

inline std::string http_date(std::time_t ts)
{
	if (struct tm *tm = gmtime(&ts))
	{
		char buf[256];

		if (strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm) != 0)
		{
			return buf;
		}
	}
	return std::string();
}

} // namespace x0
