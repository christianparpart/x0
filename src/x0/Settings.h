/* <Settings.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_settings_hpp
#define sw_x0_settings_hpp (1)

#include <x0/Api.h>
#include <x0/Property.h>
#include <boost/variant.hpp>
#include <system_error>
#include <lua.hpp>
#include <string>
#include <vector>
#include <map>

// {{{ SettingsError
namespace x0
{
	enum class SettingsError
	{
		Success,
		Unknown,
		InvalidCast,
		NotFound
	};

	const std::error_category& settingsErrorCategory() throw();

	inline std::error_code make_error_code(SettingsError ec)
	{
		return std::error_code(static_cast<int>(ec), settingsErrorCategory());
	}

	inline std::error_condition make_error_condition(SettingsError ec)
	{
		return std::error_condition(static_cast<int>(ec), settingsErrorCategory());
	}
}

namespace std
{
	// implicit conversion from gai_error to error_code
	template<> struct is_error_code_enum<x0::SettingsError> : public true_type {};
}
// }}}

namespace x0 {

//! \addtogroup base
//@{

class SettingsValue;

/** common base class for \p Settings and \p SettingsValue to share certain properties.
 * \see Settings, SettingsValue
 */
class X0_API SettingsScope {
public:
	virtual const SettingsValue operator[](const std::string& key) const = 0;
	virtual SettingsValue operator[](const std::string& key) = 0;

	virtual bool contains(const std::string& fieldname) const = 0;
};

/** object storing Settings ((complex) key/value pairs), loadable via LUA-based config files.
 *
 * \see SettingsValue
 */
class X0_API Settings :
	public SettingsScope {
private:
	Settings(const Settings&) = delete;
	Settings& operator=(const Settings&) = delete;

	lua_State *L_;
	bool owner_;

public:
	class value;

public:
	explicit Settings(const std::string& filename = "");
	explicit Settings(lua_State *L, bool owner = false);
	virtual ~Settings();

	lua_State *handle() const;

	std::error_code load_file(const std::string& filename);

	// root (global) table indexing
	const SettingsValue operator[](const std::string& key) const;
	SettingsValue operator[](const std::string& key);
	bool contains(const std::string& fieldname) const;

	template<typename T>
	T get(const std::string& path, const T& defaultValue = T());

	template<typename T>
	std::error_code load(const std::string& path, T& result);

	template<class T>
	std::error_code load(const std::string& path, value_property<T>& result);

	std::vector<std::string> keys() const;
};

class X0_API SettingsValue : public SettingsScope {
private:
	lua_State *L_;
	bool root_;
	std::vector<std::string> fieldnames_;

private:
	SettingsValue(lua_State *L, bool root, const std::vector<std::string>& fieldnames);
	SettingsValue& operator=(const SettingsValue&) = delete;

	std::string lastFieldName() const;
	int tableIndex() const;

	class fetcher // {{{
	{
	private:
		lua_State *L_;
		int depth_;

	public:
		explicit fetcher(const SettingsValue&);
		~fetcher();
	}; // }}}

	friend class Settings;

public:
	SettingsValue(const SettingsValue&);
	~SettingsValue();

	bool isTable() const;
	bool isString() const;
	bool isNumber() const;
	bool isBool() const;
	bool isNull() const;

	const SettingsValue operator[](const std::string& AFieldName) const;
	SettingsValue operator[](const std::string& AFieldName);
	bool contains(const std::string& AFieldName) const;

	// value read
	template<typename T> std::error_code load(T& _value) const;
	template<typename T> std::error_code load(value_property<T>& result) const;
	template<typename T, class Object, void (Object::*Writer)(const T&)> std::error_code load(WriteProperty<T, Object, Writer>& result) const;
	template<typename T> T get(const T& _default = T()) const;
	template<typename T> T as() const;
	template<typename T> std::vector<T> keys() const;
	template<typename T> std::vector<T> values() const;

	// value write
	SettingsValue& operator=(const std::string& value);
	SettingsValue& operator=(const char *value);
	SettingsValue& operator=(int value);
	SettingsValue& operator=(long long value);
	SettingsValue& operator=(float value);
	SettingsValue& operator=(bool value);
	SettingsValue& operator=(const std::vector<std::string>& value);
	SettingsValue& operator=(const std::vector<int>& value);

private:
	template<typename T> T as(int index) const;
//	template<typename K, typename V> std::map<K, V> as(int index) const;

	template<typename K, typename V>
	inline std::map<K, V> toMap(int index) const;
};

//@}

} // namespace x0

#include <x0/Settings.cc>

#endif
