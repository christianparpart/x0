/* <x0/config.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_config_hpp
#define x0_config_hpp (1)

#include <x0/types.hpp>
#include <string>
#include <map>
#include <boost/lexical_cast.hpp>

namespace x0 {

/**
 * \ingroup core
 * \brief configuration settings API.
 */
class config {
public:
	typedef std::map<std::string, std::string> section;
	typedef std::map<std::string, section> map_type;

	typedef map_type::iterator iterator;
	typedef map_type::const_iterator const_iterator;

public:
	/// loads config settings from given filename.
	void load_file(const std::string& filename);

	/// serializes config object into INI style config file format
	std::string serialize() const;

	/// completely clears all config data.
	void clear();

	/// tests wether given section exists.
	bool contains(const std::string& title) const;

	/// gets all values of given section.
	section get(const std::string& title) const;

	/// removes a section from this config object
	void remove(const std::string& title);

	/// tests wether given data key in given section exists.
	bool contains(const std::string& title, const std::string& key) const;

	/// gets value of given section->key pair.
	std::string get(const std::string& title, const std::string& key) const;

	/// retrieves the value at (\p title - \p key) and stores it in \p result if it could be load
	template<class T, class U>
	void load(const std::string& title, const std::string& key, U& result) const;

	/// sets value of given section->key pair.
	std::string set(const std::string& title, const std::string& key, const std::string& value);

	/// removes given data by key from given section.
	void remove(const std::string& title, const std::string& key);

	/// retrieve a list of section titles matching given pattern.
	std::vector<std::string> list(const std::string& pattern);

	const_iterator cbegin() const;
	const_iterator cend() const;

private:
	mutable map_type sections;
};

template<class T, class U>
inline void config::load(const std::string& title, const std::string& key, U& result) const
{
	std::string value(get(title, key));

	if (value.size() != 0)
	{
		result = boost::lexical_cast<T>(value);
	}
}

} // namespace x0

#endif
