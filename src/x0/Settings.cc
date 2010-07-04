#include <x0/strutils.h>
#include <boost/logic/tribool.hpp>

namespace x0 {

// {{{ Settings
template<class T>
inline T Settings::get(const std::string& path, const T& defaultValue)
{
	if (contains(path))
		return SettingsValue(L_, true, split<std::string>(path, ".")).as<T>();
	else
		return defaultValue;
}

template<typename T>
inline std::error_code Settings::load(const std::string& path, T& result)
{
	if (!contains(path))
		return SettingsError::NotFound;

	result = SettingsValue(L_, true, split<std::string>(path, ".")).as<T>();
	return std::error_code();
}

template<class T>
inline std::error_code Settings::load(const std::string& path, value_property<T>& result)
{
	if (!contains(path))
		return SettingsError::NotFound;

	result = SettingsValue(L_, true, split<std::string>(path, ".")).as<T>();
	return std::error_code();
}

inline std::vector<std::string> Settings::keys() const
{
	std::vector<std::string> result;

	lua_getfield(L_, LUA_GLOBALSINDEX, "_G");
	lua_pushnil(L_); // initial key
	while (lua_next(L_, -2))
	{
		//result.push_back(as<T>(-2));
		switch (lua_type(L_, -2))
		{
			case LUA_TSTRING:
				result.push_back(lua_tostring(L_, -2));
			default:
				;
		}

		lua_pop(L_, 1); // pop value
	}

	return result;
}
// }}}

// {{{ SettingsValue
template<typename T>
std::error_code SettingsValue::load(T& _value) const
{
	fetcher _(*this);

	if (lua_type(L_, -1) == LUA_TNIL)
		return SettingsError::NotFound;

	_value = this->as<T>(-1);
	return std::error_code();
}

template<typename T>
inline std::error_code SettingsValue::load(value_property<T>& result) const
{
	fetcher _(*this);

	if (lua_type(L_, -1) == LUA_TNIL)
		return SettingsError::NotFound;

	result = this->as<T>(-1);
	return std::error_code();
}

template<typename T, class Object, void (Object::*Writer)(const T&)>
inline std::error_code SettingsValue::load(WriteProperty<T, Object, Writer>& result) const
{
	fetcher _(*this);

	if (lua_type(L_, -1) == LUA_TNIL)
		return SettingsError::NotFound;

	result = this->as<T>(-1);
	return std::error_code();
}

template<typename T>
T SettingsValue::get(const T& _default) const
{
	T result = T();
	return load(result) ? result : _default;
}

template<class T>
inline T SettingsValue::as() const
{
	fetcher _(*this);
	return this->as<T>(-1);
}

template<typename T>
inline std::vector<T> SettingsValue::keys() const
{
	fetcher _(*this);

	std::vector<T> result;

	switch (lua_type(L_, -1))
	{
		case LUA_TTABLE:
			break;
		case LUA_TNIL:
		default:
			return result;
	}

	lua_pushnil(L_); // initial key
	while (lua_next(L_, -2))
	{
		result.push_back(as<T>(-2));
		lua_pop(L_, 1); // pop value
	}

	return result;
}

template<typename T>
std::vector<T> SettingsValue::values() const
{
	return as<std::vector<T>>();
}

// {{{ SettingsValue::as<T>(int index)
template<>
inline std::string SettingsValue::as<std::string>(int index) const
{
	switch (lua_type(L_, index)) {
		case LUA_TNIL:
			return std::string();
		case LUA_TNUMBER:
		case LUA_TSTRING:
			return lua_tostring(L_, index);
		case LUA_TBOOLEAN:
			return lua_toboolean(L_, index) ? "true" : "false";
		default:
			throw "Cast Error: Expected `string`.";
	}
}

template<>
inline int SettingsValue::as<int>(int index) const
{
	if (!lua_isnumber(L_, index))
		throw "Cast Error: Expected `float`.";

	return (int)lua_tonumber(L_, index);
}

template<>
inline long long SettingsValue::as<long long>(int index) const
{
	if (!lua_isnumber(L_, index))
		throw "Cast Error: Expected `float`.";

	return (long long)lua_tonumber(L_, index);
}

template<>
inline std::size_t SettingsValue::as<std::size_t>(int index) const
{
	if (!lua_isnumber(L_, index))
		throw "Cast Error: Expected `float`.";

	return (std::size_t)lua_tonumber(L_, index);
}

template<>
inline float SettingsValue::as<float>(int index) const
{
	if (!lua_isnumber(L_, index))
		throw "Cast Error: Expected `float`.";

	return (float)lua_tonumber(L_, index);
}

template<>
inline bool SettingsValue::as<bool>(int index) const
{
	if (!lua_isboolean(L_, index))
		throw "cast error: expected `boolean`.";

	return lua_toboolean(L_, index);
}

template<>
inline boost::tribool SettingsValue::as<boost::tribool>(int index) const
{
	if (!lua_isboolean(L_, index))
		throw "cast error: expected `boolean`.";

	return lua_toboolean(L_, index);
}

template<>
inline std::vector<std::string> SettingsValue::as<std::vector<std::string>>(int index) const
{
	std::vector<std::string> result;

	if (lua_isnil(L_, index))
	{
		return result;
	}
	else if (lua_isstring(L_, index))
	{
		result.push_back(lua_tostring(L_, index));
		return result;
	}
	else if (lua_istable(L_, index))
	{
		for (int i = 1, n = luaL_getn(L_, index); i <= n; ++i)
		{
			lua_rawgeti(L_, index, i);
			result.push_back(lua_tostring(L_, -1));
			lua_pop(L_, 1);
		}
		return result;
	}
	throw "cast error: expected `table`.";
}

template<>
inline std::vector<int> SettingsValue::as<std::vector<int>>(int index) const
{
	std::vector<int> result;

	if (lua_isnil(L_, index))
	{
		return result;
	}
	else if (lua_isnumber(L_, index))
	{
		result.push_back(lua_tonumber(L_, index));
		return result;
	}
	else if (lua_istable(L_, index))
	{
		for (int i = 1, n = luaL_getn(L_, index); i <= n; ++i)
		{
			lua_rawgeti(L_, index, i);
			result.push_back(lua_tonumber(L_, -1));
			lua_pop(L_, 1);
		}
		return result;
	}
	throw "cast error: expected `table`.";
}

template<>
inline std::map<std::string, std::string> SettingsValue::as<std::map<std::string, std::string>>(int index) const
{
	fetcher _(*this);
	return toMap<std::string, std::string>(-1);
}

template<>
inline std::map<int, std::string> SettingsValue::as<std::map<int, std::string>>(int index) const
{
	fetcher _(*this);
	return toMap<int, std::string>(-1);
}

template<typename K, typename V>
inline std::map<K, V> SettingsValue::toMap(int index) const
{
	std::map<K, V> result;

	switch (lua_type(L_, index))
	{
		case LUA_TNIL:
			return result;
		case LUA_TTABLE:
			break;
		default:
			throw "Cast Error: Expected `table`.";
	}

	lua_pushnil(L_); // push dump-key
	while (lua_next(L_, index - 1))
	{
		// key at -2, value at -1
		result[as<K>(-2)] = as<V>(-1);

		lua_pop(L_, 1); // pop value, keep key for next iteration
	}

	return result;
}
// }}}
// }}}

} // namespace x0

// vim:syntax=cpp
