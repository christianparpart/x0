/* <x0/IniFile.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/IniFile.h>
#include <x0/StringTokenizer.h>

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

static inline std::string trim(const std::string value)
{
	std::size_t left = 0;
	while (std::isspace(value[left]))
		++left;

	std::size_t right = value.size() - 1;
	while (std::isspace(value[right]))
		--right;

	return std::string(value.data() + left, value.data() + right + 1);
}

IniFile::IniFile() :
	sections_()
{
}

IniFile::~IniFile()
{
}

bool IniFile::loadFile(const std::string& filename)
{
	std::string current_title;
	std::ifstream ifs(filename);

	while (ifs.good()) {
		char buf[4096];
		ifs.getline(buf, sizeof(buf));

		std::string value(trim(buf));

		if (value.empty() || value[0] == ';' || value[0] == '#') {
			continue;
		} else if (value[0] == '[' && value[value.size() - 1] == ']') {
			current_title = value.substr(1, value.size() - 2);
		} else if (!current_title.empty()) {
			size_t eq = value.find('=');

			if (eq != std::string::npos) {
				std::string lhs = trim(value.substr(0, eq));
				std::string rhs = trim(value.substr(eq + 1));

				sections_[current_title][lhs] = rhs;
			} else {
				sections_[current_title][value] = std::string();
			}
		} else {
			// TODO throw instead
			fprintf(stderr, "unplaced data. '%s'\n", value.c_str());
			errno = EINVAL;
			return false;
		}
	}
	return true;
}

std::string IniFile::serialize() const
{
	std::stringstream sstr;

	for (auto s = sections_.cbegin(); s != sections_.cend(); ++s) {
		sstr << '[' << s->first << ']' << std::endl;

		auto sec = s->second;

		for (auto row = sec.cbegin(); row != sec.cend(); ++row) {
			sstr << row->first << '=' << row->second << std::endl;
		}

		sstr << std::endl;
	}

	return sstr.str();
}

void IniFile::clear()
{
	sections_.clear();
}

bool IniFile::contains(const std::string& section) const
{
	return sections_.find(section) != sections_.end();
}

IniFile::Section IniFile::get(const std::string& title) const
{
	if (contains(title)) {
		return sections_[title];
	} else {
		return Section();
	}
}

void IniFile::remove(const std::string& title)
{
	sections_.erase(title);
}

bool IniFile::contains(const std::string& title, const std::string& key) const
{
	auto i = sections_.find(title);

	if (i != sections_.end())
	{
		auto s = i->second;

		if (s.find(key) != s.end())
		{
			return true;
		}
	}
	return false;
}

std::string IniFile::get(const std::string& title, const std::string& key) const
{
	auto i = sections_.find(title);

	if (i != sections_.end()) {
		auto s = i->second;
		auto k = s.find(key);

		if (k != s.end()) {
			return k->second;
		}
	}
	return std::string();
}

bool IniFile::load(const std::string& title, const std::string& key, std::string& result) const
{
	auto i = sections_.find(title);

	if (i == sections_.end())
		return false;

	auto s = i->second;
	auto k = s.find(key);

	if (k == s.end()) 
		return false;

	result = k->second;
	return true;
}

std::string IniFile::set(const std::string& title, const std::string& key, const std::string& value)
{
	return sections_[title][key] = value;
}

void IniFile::remove(const std::string& title, const std::string& key)
{
	auto si = sections_.find(title);
	if (si != sections_.end())
	{
		auto s = si->second;
		if (s.find(key) != s.end())
		{
			s.erase(key);
		}
	}
}

} // namespace x0
