/* <x0/config.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/config.hpp>
#include <x0/strutils.hpp>
#include <boost/tokenizer.hpp>
#include <exception>
#include <iostream>

namespace x0 {

void config::load_file(const std::string& filename)
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

	std::string input(read_file(filename));
	tokenizer lines(input, boost::char_separator<char>("\n"));
	std::string current_title;

	for (tokenizer::iterator line = lines.begin(); line != lines.end(); ++line)
	{
		std::string value(trim(*line));

		if (value.empty() || value[0] == ';')
		{
			continue;
		}
		else if (value[0] == '[' && value[value.size() - 1] == ']')
		{
			current_title = value.substr(1, value.size() - 2);
		}
		else if (!current_title.empty())
		{
			std::size_t eq = value.find('=');

			if (eq != std::string::npos)
			{
				std::string lhs = trim(value.substr(0, eq));
				std::string rhs = trim(value.substr(eq + 1));

				sections[current_title][lhs] = rhs;
			}
			else
			{
				sections[current_title][value] = std::string();
			}
		}
		else
		{
			std::cerr << "unplaced data: " << value;
		}
	}
}

std::string config::serialize() const
{
	std::stringstream sstr;

	for (map_type::const_iterator s = sections.begin(); s != sections.end(); ++s)
	{
		sstr << '[' << s->first << ']' << std::endl;

		const section& sec = s->second;

		for (section::const_iterator row = sec.begin(); row != sec.end(); ++row)
		{
			sstr << row->first << '=' << row->second << std::endl;
		}

		sstr << std::endl;
	}

	return sstr.str();
}

void config::clear()
{
	sections.clear();
}

bool config::contains(const std::string& section) const
{
	return sections.find(section) != sections.end();
}

config::section config::get(const std::string& title) const
{
	if (contains(title))
	{
		return sections[title];
	}
	else
	{
		return section();
	}
}

void config::remove(const std::string& title)
{
	sections.erase(title);
}

bool config::contains(const std::string& title, const std::string& key) const
{
	const_iterator i = sections.find(title);

	if (i != sections.end())
	{
		const section& s = i->second;

		if (s.find(key) != s.end())
		{
			return true;
		}
	}
	return false;
}

std::string config::get(const std::string& title, const std::string& key) const
{
	map_type::const_iterator i = sections.find(title);

	if (i != sections.end())
	{
		const section& s = i->second;
		section::const_iterator k = s.find(key);

		if (k != s.end())
		{
			return k->second;
		}
	}
	return std::string();
}

std::string config::set(const std::string& title, const std::string& key, const std::string& value)
{
	return sections[title][key] = value;
}

void config::remove(const std::string& title, const std::string& key)
{
	map_type::iterator si = sections.find(title);
	if (si != sections.end())
	{
		section& s = si->second;
		if (s.find(key) != s.end())
		{
			s.erase(key);
		}
	}
}

config::const_iterator config::cbegin() const
{
	return sections.begin();
}

config::const_iterator config::cend() const
{
	return sections.end();
}

} // namespace x0
