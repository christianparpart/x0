/* <x0/plugin.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0/types.hpp>
#include <x0/http/server.hpp>
#include <x0/api.hpp>

namespace x0 {

//! \addtogroup core
//@{

class server;

/**
 * \brief base class for all plugins for use within this x0 web server.
 *
 * \see server, connection, request, response
 */
class plugin
{
private:
	plugin(const plugin&) = delete;
	plugin& operator=(const plugin&) = delete;

public:
	plugin(server& srv, const std::string& name);
	~plugin();

	std::string name() const;

	virtual void post_config();

	template<typename... Args>
	inline void log(severity sv, const char *msg, Args&&... args);

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args);

#if !defined(NDEBUG)
	inline int debug_level() const;
	void debug_level(int value);
#endif

	template<typename T>
	void register_cvar(const std::string& key, context mask,
			bool (T::*handler)(const settings_value&, scope&), int priority = 0);

	const std::vector<std::string>& cvars() const;
	void unregister_cvar(const std::string& key);

protected:
	server& server_;
	std::string name_;
	std::vector<std::string> cvars_;

#if !defined(NDEBUG)
	int debug_level_;
#endif
};

// {{{ inlines
/** retrieves the plugin's unique basename (index, userdir, sendfile, auth, etc...) */
inline std::string plugin::name() const
{
	return name_;
}

template<typename... Args>
inline void plugin::log(severity sv, const char *msg, Args&&... args)
{
	server_.log(sv, msg, args...);
}

template<typename... Args>
inline void plugin::debug(int level, const char *msg, Args&&... args)
{
#if !defined(NDEBUG)
	if (level <= debug_level_)
	{
		buffer fmt;
		fmt.push_back(name_);
		fmt.push_back(": ");
		fmt.push_back(msg);

		server_.log(severity::debug, fmt.c_str(), args...);
	}
#endif
}

#if !defined(NDEBUG)
inline int plugin::debug_level() const
{
	return debug_level_;
}

inline void plugin::debug_level(int value)
{
	debug_level_ = value;
}

/** \brief registers a configuration variable handler.
  *
  * \param key configuration variable name
  * \param mask context mask (OR-ed together) describing in which contexts this variable may occur
  * \param handler callback handler to be invoked on occurence.
  * \param priority handler-invokation priority. the higher the later.
  *
  * \see server::register_cvar()
  */
template<typename T>
inline void plugin::register_cvar(const std::string& key, context mask,
	bool (T::*handler)(const settings_value&, scope&), int priority)
{
	cvars_.push_back(key);
	server_.register_cvar(key, mask, std::bind(handler, static_cast<T *>(this), std::placeholders::_1, std::placeholders::_2), priority);
}
#endif
// }}}

#define X0_EXPORT_PLUGIN(pluginName) \
	X0_EXPORT_PLUGIN_CLASS(pluginName, pluginName##_plugin)

#define X0_EXPORT_PLUGIN_CLASS(pluginName, className) \
	extern "C" x0::plugin *x0plugin_init(x0::server& srv, const std::string& name) { \
		return new className(srv, name); \
	}

//@}

} // namespace x0

#endif
