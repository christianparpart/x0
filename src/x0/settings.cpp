#include <x0/settings.hpp>
#include <x0/strutils.hpp>
#include <stdexcept>

#include <lua.hpp>
#include <cstdio>

namespace x0 {

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

// {{{ settings
settings::settings(const std::string& filename) :
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

settings::settings(lua_State *_L, bool _owner) :
	L_(_L), owner_(_owner)
{
}

settings::~settings()
{
	if (owner_) {
		lua_close(L_);
		L_ = 0;
		owner_ = 0;
	}
}

void settings::load_file(const std::string& filename)
{
	int ec = luaL_dofile(L_, filename.c_str());
	if (ec)
	{
		std::string message(lua_tostring(L_, -1));
		lua_pop(L_, 1);

		throw std::runtime_error(message);
	}
}

lua_State *settings::handle() const
{
	return L_;
}

template<class T> static inline std::vector<T> make_vector(const T& v1)
{
	std::vector<T> result;
	result.push_back(v1);
	return result;
}

const settings_value settings::operator[](const std::string& _key) const
{
	return settings_value(L_, true, make_vector(_key));
}

settings_value settings::operator[](const std::string& _key)
{
	return settings_value(L_, true, make_vector(_key));
}

bool settings::contains(const std::string& _fieldname) const
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

// {{{ settings_value::fetcher
settings_value::fetcher::fetcher(const settings_value& value) : 
		L_(value.L_), depth_(value.fieldnames_.size())
{
	//dumpStack(L_, std::string::format("settings_value.fetch(name={0}, top={1})", value.fieldname_, lua_gettop(L_)).c_str());

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

settings_value::fetcher::~fetcher()
{
	lua_pop(L_, depth_);
}
// }}}

// {{{ settings_value
settings_value::settings_value(lua_State *_L, bool _root, const std::vector<std::string>& _fieldnames) :
	L_(_L),
	root_(_root),
	fieldnames_(_fieldnames)
{
}

settings_value::settings_value(const settings_value& v) :
	L_(v.L_),
	root_(v.root_),
	fieldnames_(v.fieldnames_)
{
}

settings_value::~settings_value()
{
}

std::string settings_value::lastFieldName() const
{
	return fieldnames_[fieldnames_.size() - 1];
}

int settings_value::tableIndex() const
{
	return root_ && fieldnames_.size() == 1 ? LUA_GLOBALSINDEX : -3;
}

const settings_value settings_value::operator[](const std::string& _fieldname) const
{
	std::vector<std::string> names(fieldnames_);
	names.push_back(_fieldname);

	return settings_value(L_, true, names);
}

settings_value settings_value::operator[](const std::string& _fieldname)
{
	std::vector<std::string> names(fieldnames_);
	names.push_back(_fieldname);

	return settings_value(L_, true, names);
}

bool settings_value::contains(const std::string& _fieldname) const
{
	fetcher _(*this);

	if (!lua_istable(L_, -1))
		return false;

	lua_getfield(L_, -1, _fieldname.c_str());
	bool result = !lua_isnil(L_, -1);
	lua_pop(L_, 1);
	return result;
}

settings_value& settings_value::operator=(const std::string& value)
{
	return operator=(value.c_str());
}

settings_value& settings_value::operator=(const char *value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushstring(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushstring(L_, value);

	return *this;
}

settings_value& settings_value::operator=(int value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushinteger(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushinteger(L_, value);

	return *this;
}

settings_value& settings_value::operator=(long long value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushinteger(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushinteger(L_, value);

	return *this;
}

settings_value& settings_value::operator=(float value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushnumber(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushnumber(L_, value);

	return *this;
}

settings_value& settings_value::operator=(bool value)
{
	fetcher _(*this);

	lua_pop(L_, 1);
	lua_pushstring(L_, lastFieldName().c_str());
	lua_pushboolean(L_, value);
	lua_settable(L_, tableIndex());
	lua_pushboolean(L_, value);

	return *this;
}

settings_value& settings_value::operator=(const std::vector<std::string>& value)
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
