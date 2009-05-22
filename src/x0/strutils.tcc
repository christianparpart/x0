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
std::list<T> split(const std::basic_string<U>& input, const std::basic_string<U>& sep)
{
	std::list<T> result;
	boost::tokenizer<boost::char_separator<char> > tk(input, boost::char_separator<U>(sep.c_str()));

	for (auto i = tk.begin(), e = tk.end(); i != e; ++i)
	{
		result.push_back(boost::lexical_cast<T>(*i));
	}

	return result;
}

template<typename T, typename U>
std::list<T> split(const std::basic_string<U>& list, const U *sep)
{
	return split<T, U>(list, std::basic_string<U>(sep));
}

} // namespace x0
