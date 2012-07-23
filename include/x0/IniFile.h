/* <x0/IniFile.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include <x0/Api.h>
#include <string>
#include <map>

namespace x0 {

class X0_API IniFile {
public:
	typedef std::map<std::string, std::string> Section;
	typedef std::map<std::string, Section> SectionMap;

	typedef SectionMap::iterator iterator;
	typedef SectionMap::const_iterator const_iterator;

public:
	IniFile();
	~IniFile();

	/// loads config settings from given filename.
	bool loadFile(const std::string& filename);

	/// serializes config object into INI style config file format
	std::string serialize() const;

	/// completely clears all config data.
	void clear();

	/// tests wether given section exists.
	bool contains(const std::string& title) const;

	/// gets all values of given section.
	Section get(const std::string& title) const;

	/// removes a section from this config object
	void remove(const std::string& title);

	/// tests wether given data key in given section exists.
	bool contains(const std::string& title, const std::string& key) const;

	/// gets value of given section->key pair.
	std::string get(const std::string& title, const std::string& key) const;

	/// sets value of given section->key pair.
	std::string set(const std::string& title, const std::string& key, const std::string& value);

	/// removes given data by key from given section.
	void remove(const std::string& title, const std::string& key);

	const_iterator cbegin() const;
	const_iterator cend() const;

private:
	mutable SectionMap sections_;
};

} // namespace x0
