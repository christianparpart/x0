#ifndef sw_x0_settings_hpp
#define sw_x0_settings_hpp (1)

#include <x0/api.hpp>
#include <x0/property.hpp>
#include <boost/noncopyable.hpp>
#include <boost/variant.hpp>
#include <lua.hpp>
#include <string>
#include <vector>
#include <map>

namespace x0 {

//! \addtogroup base
//@{

class settings_value;

/** common base class for \p settings and \p settings_value to share certain properties.
 * \see settings, settings_value
 */
class X0_API settings_scope {
public:
	virtual const settings_value operator[](const std::string& key) const = 0;
	virtual settings_value operator[](const std::string& key) = 0;

	virtual bool contains(const std::string& fieldname) const = 0;
};

/** object storing settings ((complex) key/value pairs), loadable via LUA-based config files.
 *
 * \see settings_value
 */
class X0_API settings :
	public boost::noncopyable,
	public settings_scope {
private:
	lua_State *L_;
	bool owner_;

public:
	class value;

public:
	explicit settings(const std::string& filename = "");
	explicit settings(lua_State *L, bool owner = false);
	virtual ~settings();

	lua_State *handle() const;

	void load_file(const std::string& filename);

	// root (global) table indexing
	const settings_value operator[](const std::string& key) const;
	settings_value operator[](const std::string& key);
	bool contains(const std::string& fieldname) const;

	template<typename T>
	T get(const std::string& path, const T& defaultValue = T());

	template<typename T>
	bool load(const std::string& path, T& result);

	template<class T>
	bool load(const std::string& path, value_property<T>& result);
};

class X0_API settings_value : public settings_scope {
private:
	lua_State *L_;
	bool root_;
	std::vector<std::string> fieldnames_;

private:
	settings_value(lua_State *L, bool root, const std::vector<std::string>& fieldnames);
	settings_value& operator=(const settings_value&) = delete;

	std::string lastFieldName() const;
	int tableIndex() const;

	class fetcher // {{{
	{
	private:
		lua_State *L_;
		int depth_;

	public:
		explicit fetcher(const settings_value&);
		~fetcher();
	}; // }}}

	friend class settings;

public:
	settings_value(const settings_value&);
	~settings_value();

	// table indexing
	const settings_value operator[](const std::string& AFieldName) const;
	settings_value operator[](const std::string& AFieldName);
	bool contains(const std::string& AFieldName) const;

	// value read
	template<typename T> bool load(T& _value);
	template<typename T> T get(const T& _default);
	template<typename T> T as() const;
	template<typename T> std::vector<T> keys() const;

	// value write
	settings_value& operator=(const std::string& value);
	settings_value& operator=(const char *value);
	settings_value& operator=(int value);
	settings_value& operator=(long long value);
	settings_value& operator=(float value);
	settings_value& operator=(bool value);
	settings_value& operator=(const std::vector<std::string>& value);
	settings_value& operator=(const std::vector<int>& value);

private:
	template<typename T> T as(int index) const;
//	template<typename K, typename V> std::map<K, V> as(int index) const;

	template<typename K, typename V>
	inline std::map<K, V> toMap(int index) const;
};

//@}

} // namespace x0

#include <x0/settings.ipp>

#endif
