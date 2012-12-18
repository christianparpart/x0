/* <HttpPlugin.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0/Api.h>
#include <x0/http/HttpServer.h>
#include <x0/Types.h>
#include <x0/Defines.h>
#include <x0/Api.h>
#include <x0/flow/Flow.h>
#include <x0/flow/FlowValue.h>

#include <string>
#include <vector>
#include <system_error>

namespace x0 {

//! \addtogroup http
//@{

/**
 * \brief base class for all plugins for use within this x0 web server.
 *
 * \see server, connection, request, response
 */
class X0_API HttpPlugin
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
	virtual void cycleLogs();

	template<typename... Args>
	inline void log(Severity sv, const char *msg, Args&&... args);

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args);

#if !defined(NDEBUG)
	inline int debug_level() const;
	void debug_level(int value);
#endif

	HttpServer& server() const;

protected:
	template<typename T, void (T::*cb)(const FlowParams&, FlowValue&)> void registerSetupProperty(const std::string& name, FlowValue::Type resultType);
	template<typename T, void (T::*cb)(const FlowParams&, FlowValue&)> void registerSetupFunction(const std::string& name, FlowValue::Type resultType = FlowValue::VOID);

	template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)> void registerSharedProperty(const std::string& name, FlowValue::Type resultType);
	template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)> void registerSharedFunction(const std::string& name, FlowValue::Type resultType = FlowValue::VOID);

	template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)> void registerProperty(const std::string& name, FlowValue::Type resultType);
	template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)> void registerFunction(const std::string& name, FlowValue::Type resultType = FlowValue::VOID);
	template<typename T, bool (T::*cb)(HttpRequest*, const FlowParams&)> void registerHandler(const std::string& name);

protected:
	HttpServer& server_;
	std::string name_;

#if !defined(NDEBUG)
	int debug_level_;
#endif 
private:
	template<class T, void (T::*cb)(const FlowParams&, FlowValue&)> static void setup_thunk(void* p, FlowArray& args, void* cx);
	template<class T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)> static void method_thunk(void* p, FlowArray& args, void* cx);
	template<class T, bool (T::*cb)(HttpRequest*, const FlowParams&)> static void handler_thunk(void* p, FlowArray& args, void* cx);

	friend class HttpServer;
};

// {{{ flow integration
// setup properties
template<typename T, void (T::*cb)(const FlowParams&, FlowValue&)>
void HttpPlugin::registerSetupProperty(const std::string& name, FlowValue::Type resultType)
{
	server_.registerSetupProperty(name, resultType, &setup_thunk<T, cb>, static_cast<T*>(this));
}

// setup functions
template<typename T, void (T::*cb)(const FlowParams&, FlowValue&)>
void HttpPlugin::registerSetupFunction(const std::string& name, FlowValue::Type resultType)
{
	server_.registerSetupFunction(name, resultType, &setup_thunk<T, cb>, static_cast<T*>(this));
}

template<class T, void (T::*cb)(const FlowParams&, FlowValue&)>
void HttpPlugin::setup_thunk(void* p, FlowArray& args, void* /*cx*/)
{
	FlowParams pargs(args.size() - 1, &args[1]);
	(static_cast<T*>(p)->*cb)(pargs, args[0]);
}

// shared
template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)>
void HttpPlugin::registerSharedProperty(const std::string& name, FlowValue::Type resultType)
{
	server_.registerSharedProperty(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)>
void HttpPlugin::registerSharedFunction(const std::string& name, FlowValue::Type resultType)
{
	server_.registerSharedFunction(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

// main properties
template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)>
void HttpPlugin::registerProperty(const std::string& name, FlowValue::Type resultType)
{
	server_.registerProperty(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

// main functions
template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)>
void HttpPlugin::registerFunction(const std::string& name, FlowValue::Type resultType)
{
	server_.registerFunction(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

template<typename T, void (T::*cb)(HttpRequest*, const FlowParams&, FlowValue&)>
void HttpPlugin::method_thunk(void* p, FlowArray& args, void* cx)
{
	FlowParams pargs(args.size() - 1, &args[1]);
	(static_cast<T*>(p)->*cb)(static_cast<HttpRequest*>(cx), pargs, args[0]);
}

// main handler
template<typename T, bool (T::*cb)(HttpRequest *, const FlowParams&)>
void HttpPlugin::registerHandler(const std::string& name)
{
	server_.registerHandler(name, &handler_thunk<T, cb>, static_cast<T*>(this));
}

template<typename T, bool (T::*cb)(HttpRequest *, const FlowParams& args)>
void HttpPlugin::handler_thunk(void *p, FlowArray& args, void *cx)
{
	FlowParams pargs(args.size() - 1, &args[1]);
	args[0].set((static_cast<T*>(p)->*cb)(static_cast<HttpRequest*>(cx), pargs));
}
// }}}

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
	Buffer fmt;
	fmt.push_back(name_);
	fmt.push_back(": ");
	fmt.push_back(msg);

	server_.log(sv, fmt.c_str(), args...);
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
#endif
// }}}

#define X0_EXPORT_PLUGIN(pluginName) \
	X0_EXPORT_PLUGIN_CLASS(pluginName##_plugin)

#define X0_EXPORT_PLUGIN_CLASS(className) \
	extern "C" X0_EXPORT x0::HttpPlugin *x0plugin_init(x0::HttpServer& srv, const std::string& name) { \
		return new className(srv, name); \
	}

//@}

} // namespace x0

#endif
