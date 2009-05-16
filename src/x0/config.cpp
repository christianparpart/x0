#include <x0/config.hpp>
#include <boost/tokenizer.hpp>
#include <exception>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cctype>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace x0 {

static std::string read_file(const std::string& filename)
{
#if 0
	// XXX gcc 4.4.0: ld error, can't resolv fstream.constructor or fstream.open - wtf?
	std::fstream ifs(filename);
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
			if (read(fd, buf, st.st_size) != -1)
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

void config::load_file(const std::string& filename)
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

	std::string input(read_file(filename));
	tokenizer lines(input, boost::char_separator<char>("\n"));
	std::string current_title;

	for (auto line = lines.begin(); line != lines.end(); ++line)
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

	for (auto s = sections.cbegin(); s != sections.cend(); ++s)
	{
		sstr << '[' << s->first << ']' << std::endl;

		auto sec = s->second;

		for (auto row = sec.cbegin(); row != sec.cend(); ++row)
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
	auto i = sections.find(title);

	if (i != sections.end())
	{
		auto s = i->second;

		if (s.find(key) != s.end())
		{
			return true;
		}
	}
	return false;
}

std::string config::get(const std::string& title, const std::string& key) const
{
	auto i = sections.find(title);

	if (i != sections.end())
	{
		auto s = i->second;
		auto k = s.find(key);

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
	auto si = sections.find(title);
	if (si != sections.end())
	{
		auto s = si->second;
		if (s.find(key) != s.end())
		{
			s.erase(key);
		}
	}
}

config::const_iterator config::cbegin() const
{
	return sections.cbegin();
}

config::const_iterator config::cend() const
{
	return sections.cend();
}

} // namespace x0
