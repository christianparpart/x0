/* <x0/Settings.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Settings.h>
#include <x0/StringError.h>
#include <x0/strutils.h>
#include <stdexcept>

#include <lua.hpp>
#include <cstdio>

namespace x0 {

// {{{ SettingsError
class SettingsErrorCategoryImpl :
	public std::error_category
{
public:
	SettingsErrorCategoryImpl()
	{
	}

	virtual const char *name() const
	{
		return "SettingsError";
	}

	virtual std::string message(int ec) const
	{
		switch (ec)
		{
			case SettingsError::Success:
				return "success";
			case SettingsError::InvalidCast:
				return "invalid cast";
			case SettingsError::NotFound:
				return "not found";
			case SettingsError::Unknown:
			default:
				return "unknown";
		}
	}
};

const std::error_category& settingsErrorCategory() throw()
{
	static SettingsErrorCategoryImpl impl;
	return impl;
}
// }}}

void dumpStack(lua_State *L_, const char *msg = 0) // {{{
{
	int top = lua_gettop(L_);

	printf("LUA stack dump (%d): %s\n", top, msg ? msg : "");

	for (int i = top; i > 0; --i) {
		char value[256];
		switch (lua_type(L_, i)) {
			case LUA_TNONE:
				snprintf(value, sizeof(value), "none");
				break;
			case LUA_TNIL:
				snprintf(value, sizeof(value), "nil");
				break;
			case LUA_TBOOLEAN:
				snprintf(value, sizeof(value), lua_toboolean(L_, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:
				snprintf(value, sizeof(value), "%f", lua_tonumber(L_, i));
				break;
			case LUA_TSTRING:
				snprintf(value, sizeof(value), "%s", lua_tostring(L_, i));
				break;
			case LUA_TTABLE:
				snprintf(value, sizeof(value), "%s", lua_tostring(L_, i));
				break;
			default:
				snprintf(value, sizeof(value), "%s", lua_tostring(L_, i));
				break;
		}
		printf(" [%3d] %s (%s)\n", (i - top - 1), value, lua_typename(L_, lua_type(L_, i)));
	}
} // }}}

// {{{ Settings
Settings::Settings(const std::string& filename) :
	L_(luaL_newstate()), owner_(true)
{
#if 0
	loaopen_base(L_);
	luaopen_table(L_);
	luaopen_io(L_);
	luaopen_os(L_);
	luaopen_string(L_);
	luaopen_math(L_);
	luaopen_debug(L_);
	luaopen_package(L_);
#else
	luaL_openlibs(L_);
#endif

	if (!filename.empty())
	{
		load_file(filename);
	}
}

Settings::Settings(lua_State *_L, bool _owner) :
	L_(_L), owner_(_owner)
{
}

Settings::~Settings()
{
	if (owner_) {
		lua_close(L_);
		L_ = 0;
		owner_ = 0;
	}
}

std::error_code Settings::load_file(const std::string& filename)
{
	int ec = luaL_dofile(L_, filename.c_str());
	if (!ec)
		return std::error_code();

	std::string message(lua_tostring(L_, -1));
	lua_pop(L_, 1);

	return make_error_code(message);
}

lua_State *Settings::handle() const
{
	return L_;
}

template<class T> static inline std::vector<T> make_vector(const T& v1)
{
	std::vector<T> result;
	result.push_back(v1);
	return result;
}

const SettingsValue Settings::operator[](const std::string& _key) const
{
	return SettingsValue(L_, true, make_vector(_key));
}

SettingsValue Settings::operator[](const std::string& _key)
{
	return SettingsValue(L_, true, make_vector(_key));
}

bool Settings::contains(const std::string& _fieldname) const
{
	std::vector<std::string> atoms(split<std::string>(_fieldname, "."));

	lua_getfield(L_, LUA_GLOBALSINDEX, atoms[0].c_str());
	bool found = !lua_isnil(L_, -1);
	int depth = 1;

	int n = atoms.size();

	if (n > 1) {
		while (depth < n && found && (found = lua_istable(L_, -1))) {
			lua_getfield(L_, -1, atoms[depth++].c_str());
			found = !lua_isnil(L_, -1);
		}
	}

	lua_pop(L_, depth);
	return found;
}
// }}}

// {{{ SettingsValue::fetcher
SettingsValue::fetcher::fetcher(const SettingsValue& value) : 
		L_(value.L_), depth_(value.fieldnames_.size())
{
	//dumpStack(L_, std::string::format("SettingsValue.fetch(name={0}, top={1})", value.fieldname_, lua_gettop(L_)).c_str());

	const std::vector<std::string>& atoms(value.fieldnames_);

	lua_getfield(L_, LUA_GLOBALSINDEX, atoms[0].c_str());
	if (depth_ > 1) {
		if (lua_isnil(L_, -1)) {
			lua_pop(L_, 1);
			lua_newtable(L_);
			lua_pushvalue(L_, -1);
			lua_setfield(L_, LUA_GLOBALSINDEX, atoms[0].c_str());
		}

		for (int i = 1; i < depth_; ++i) {
			lua_getfield(L_, -1, atoms[i].c_str());

			if (lua_isnil(L_, -1) && i + 1 < depth_) {
				lua_pop(L_, 1);
				lua_newtable(L_);
				lua_pushvalue(L_, -1);
				lua_setfield(L_, -3, atoms[i].c_str());
			}
		}
	}
}

SettingsValue::fetcher::~fetcher()
{
	lua_pop(L_, depth_);
}
// }}}

// {{{ SettingsValue
SettingsValue::SettingsValue(lua_State *_L, bool _root, const std::vector<std::string>& _fieldnames) :
	L_(_L),
	root_(_root),
	fieldnames_(_fieldnames)
{
}

SettingsValue::SettingsValue(const SettingsValue& v) :
	L_(v.L_),
	root_(v.root_),
	fieldnames_(v.fieldnames_)
{
}

SettingsValue::~SettingsValue()
{
}

std::string SettingsValue::lastFieldName() const
{
	return fieldnames_[fieldnames_.size() - 1];
}

int SettingsValue::tableIndex() const
{
	return root_ && fieldnames_.size() == 1 ? LUA_GLOBALSINDEX : -3;
}

const SettingsValue SettingsValue::operator[](const std::string& _fieldname) const
{
	std::vector<std::string> names(fieldnames_);
	names.push_back(_fieldname);

	return SettingsValue(L_, true, names);
}

SettingsValue SettingsValue::operator[](const std::string& _fieldname)
{
	std::vector<std::string> names(fieldnames_);
	names.push_back(_fieldname);

	return SettingsValue(L_, true, names);
}

bool SettingsValue::contains(const std::string& _fieldname) const
{
	fetcher _(*this);

	if (!lua_istable(L_, -1))
		return false;

	lua_getfield(L_, -1, _fieldname.c_str());
	bool result = !lua_isnil(L_, -1);
	lua_pop(L_, 1);
	return result;
}

SettingsValue& SettingsValue::operator=(const std::string& value)
{
	return operator=(value.c_str());
}

SettingsValue& SettingsValue::operator=(const char *value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushstring(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushstring(L_, value);

	return *this;
}

SettingsValue& SettingsValue::operator=(int value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushinteger(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushinteger(L_, value);

	return *this;
}

SettingsValue& SettingsValue::operator=(long long value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushinteger(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushinteger(L_, value);

	return *this;
}

SettingsValue& SettingsValue::operator=(float value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushnumber(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushnumber(L_, value);

	return *this;
}

SettingsValue& SettingsValue::operator=(bool value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushboolean(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushboolean(L_, value);

	return *this;
}

SettingsValue& SettingsValue::operator=(const std::vector<std::string>& value)
{
	fetcher _(*this);

	lua_pop(L_, 1);

	lua_newtable(L_);
	lua_pushvalue(L_, -1);

	for (int i = 0, n = value.size(); i < n; ) {
		lua_pushstring(L_, value[i].c_str());
		lua_rawseti(L_, -2, ++i);
	}

	lua_setfield(L_, tableIndex(), lastFieldName().c_str());

	return *this;
}
// }}}

} // namespace x0
