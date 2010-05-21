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
class context;

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
	/** initializes the plugin */
	plugin(server& srv, const std::string& name) :
#if !defined(NDEBUG)
		debug_level_(1),
#endif
		server_(srv),
		name_(name)
	{
	}

	/** finalizes the plugin */
	~plugin()
	{
	}

	/** retrieves the plugin's unique basename (index, userdir, sendfile, auth, etc...) */
	std::string name() const
	{
		return name_;
	}

	/** gets invoked at (re)configure time */
	virtual void configure()
	{
	}

	/** invoked after every plugin is loaded and configured. */
	virtual void post_config()
	{
	}

	template<typename... Args>
	inline void log(severity sv, const char *msg, Args&&... args)
	{
		server_.log(sv, msg, args...);
	}

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args)
	{
#if !defined(NDEBUG)
		if (level >= debug_level_)
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
	inline int debug_level() const
	{
		return debug_level_;
	}

	void debug_level(int value)
	{
		debug_level_ = value;
	}
#endif

protected:
#if !defined(NDEBUG)
	int debug_level_;
#endif
	server& server_;
	std::string name_;
};

#define X0_EXPORT_PLUGIN(pluginName) \
	X0_EXPORT_PLUGIN_CLASS(pluginName, pluginName##_plugin)

#define X0_EXPORT_PLUGIN_CLASS(pluginName, className) \
	extern "C" x0::plugin *x0plugin_init(x0::server& srv, const std::string& name) { \
		return new className(srv, name); \
	}

//@}

} // namespace x0

#endif
