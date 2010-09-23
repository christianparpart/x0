/* <HttpPlugin.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0/http/HttpServer.h>
#include <x0/Types.h>
#include <x0/Defines.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <system_error>

namespace x0 {

//! \addtogroup core
//@{

/**
 * \brief base class for all plugins for use within this x0 web server.
 *
 * \see server, connection, request, response
 */
class HttpPlugin
{
private:
	HttpPlugin(const HttpPlugin&) = delete;
	HttpPlugin& operator=(const HttpPlugin&) = delete;

public:
	HttpPlugin(HttpServer& srv, const std::string& name);
	virtual ~HttpPlugin();

	std::string name() const;

	virtual bool post_config();
	virtual bool post_check();

	template<typename... Args>
	inline void log(Severity sv, const char *msg, Args&&... args);

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args);

#if !defined(NDEBUG)
	inline int debug_level() const;
	void debug_level(int value);
#endif

	template<typename T>
	void declareCVar(const std::string& key, HttpContext mask,
			std::error_code (T::*handler)(const SettingsValue&, Scope&), int priority = 0);

	const std::vector<std::string>& cvars() const;
	void undeclareCVar(const std::string& key);

	HttpServer& server() const;

	virtual bool handleRequest(HttpRequest *request, HttpResponse *response);

protected:
	HttpServer& server_;
	std::string name_;
	std::vector<std::string> cvars_;

#if !defined(NDEBUG)
	int debug_level_;
#endif

	static void process(void *, int argc, Flow::Value *argv);

	friend class HttpServer;
};

// {{{ inlines
inline HttpServer& HttpPlugin::server() const
{
	return server_;
}

/** retrieves the plugin's unique basename (index, userdir, sendfile, auth, etc...) */
inline std::string HttpPlugin::name() const
{
	return name_;
}

template<typename... Args>
inline void HttpPlugin::log(Severity sv, const char *msg, Args&&... args)
{
	server_.log(sv, msg, args...);
}

template<typename... Args>
inline void HttpPlugin::debug(int level, const char *msg, Args&&... args)
{
#if !defined(NDEBUG)
	if (level <= debug_level_)
	{
		Buffer fmt;
		fmt.push_back(name_);
		fmt.push_back(": ");
		fmt.push_back(msg);

		server_.log(Severity::debug, fmt.c_str(), args...);
	}
#endif
}

#if !defined(NDEBUG)
inline int HttpPlugin::debug_level() const
{
	return debug_level_;
}

inline void HttpPlugin::debug_level(int value)
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
  * \see server::declareCVar()
  */
template<typename T>
inline void HttpPlugin::declareCVar(const std::string& key, HttpContext mask,
	std::error_code (T::*handler)(const SettingsValue&, Scope&), int priority)
{
	cvars_.push_back(key);
	server_.declareCVar(key, mask, std::bind(handler, static_cast<T *>(this), std::placeholders::_1, std::placeholders::_2), priority);
}
#endif
// }}}

#define X0_EXPORT_PLUGIN(pluginName) \
	X0_EXPORT_PLUGIN_CLASS(pluginName, pluginName##_plugin)

#define X0_EXPORT_PLUGIN_CLASS(pluginName, className) \
	extern "C" X0_EXPORT x0::HttpPlugin *x0plugin_init(x0::HttpServer& srv, const std::string& name) { \
		return new className(srv, name); \
	}

//@}

} // namespace x0

#endif
