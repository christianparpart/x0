/* <XzeroPlugin.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0d/XzeroDaemon.h>
#include <x0/http/HttpRequest.h>
#include <x0/flow/Flow.h>
#include <x0/flow/FlowValue.h>
#include <x0/Severity.h>
#include <x0/Defines.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <system_error>

namespace x0 {
	class HttpServer;
}

//! \addtogroup daemon
//@{

namespace x0d {

class XzeroDaemon;

/**
 * \brief base class for all plugins for use within this x0 web server.
 *
 * \see server, connection, request, response
 */
class X0_API XzeroPlugin
{
private:
	XzeroPlugin(const XzeroPlugin&) = delete;
	XzeroPlugin& operator=(const XzeroPlugin&) = delete;

public:
	XzeroPlugin(XzeroDaemon* daemon, const std::string& name);
	virtual ~XzeroPlugin();

	std::string name() const;

	virtual bool post_config();
	virtual bool post_check();
	virtual void cycleLogs();

	template<typename... Args>
	inline void log(x0::Severity sv, const char *msg, Args&&... args);

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args);

#if !defined(XZERO_NDEBUG)
	inline int debug_level() const;
	void debug_level(int value);
#endif

	XzeroDaemon& daemon() const;
	x0::HttpServer& server() const;

protected:
	template<typename T, void (T::*cb)(const x0::FlowParams&, x0::FlowValue&)> void registerSetupProperty(const std::string& name, x0::FlowValue::Type resultType);
	template<typename T, void (T::*cb)(const x0::FlowParams&, x0::FlowValue&)> void registerSetupFunction(const std::string& name, x0::FlowValue::Type resultType = x0::FlowValue::VOID);

	template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)> void registerSharedProperty(const std::string& name, x0::FlowValue::Type resultType);
	template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)> void registerSharedFunction(const std::string& name, x0::FlowValue::Type resultType = x0::FlowValue::VOID);

	template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)> void registerProperty(const std::string& name, x0::FlowValue::Type resultType);
	template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)> void registerFunction(const std::string& name, x0::FlowValue::Type resultType = x0::FlowValue::VOID);
	template<typename T, bool (T::*cb)(x0::HttpRequest*, const x0::FlowParams&)> void registerHandler(const std::string& name);

protected:
	XzeroDaemon* daemon_;
	x0::HttpServer* server_;
	std::string name_;

#if !defined(XZERO_NDEBUG)
	int debug_level_;
#endif 
private:
	template<class T, void (T::*cb)(const x0::FlowParams&, x0::FlowValue&)> static void setup_thunk(void* p, x0::FlowArray& args, void* cx);
	template<class T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)> static void method_thunk(void* p, x0::FlowArray& args, void* cx);
	template<class T, bool (T::*cb)(x0::HttpRequest*, const x0::FlowParams&)> static void handler_thunk(void* p, x0::FlowArray& args, void* cx);

	friend class XzeroDaemon;
};

// {{{ flow integration
// setup properties
template<typename T, void (T::*cb)(const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::registerSetupProperty(const std::string& name, x0::FlowValue::Type resultType)
{
	daemon_->registerSetupProperty(name, resultType, &setup_thunk<T, cb>, static_cast<T*>(this));
}

// setup functions
template<typename T, void (T::*cb)(const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::registerSetupFunction(const std::string& name, x0::FlowValue::Type resultType)
{
	daemon_->registerSetupFunction(name, resultType, &setup_thunk<T, cb>, static_cast<T*>(this));
}

template<class T, void (T::*cb)(const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::setup_thunk(void* p, x0::FlowArray& args, void* /*cx*/)
{
	x0::FlowParams pargs(args.size() - 1, &args[1]);
	(static_cast<T*>(p)->*cb)(pargs, args[0]);
}

// shared
template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::registerSharedProperty(const std::string& name, x0::FlowValue::Type resultType)
{
	daemon_->registerSharedProperty(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::registerSharedFunction(const std::string& name, x0::FlowValue::Type resultType)
{
	daemon_->registerSharedFunction(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

// main properties
template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::registerProperty(const std::string& name, x0::FlowValue::Type resultType)
{
	daemon_->registerProperty(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

// main functions
template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::registerFunction(const std::string& name, x0::FlowValue::Type resultType)
{
	daemon_->registerFunction(name, resultType, &method_thunk<T, cb>, static_cast<T*>(this));
}

template<typename T, void (T::*cb)(x0::HttpRequest*, const x0::FlowParams&, x0::FlowValue&)>
void XzeroPlugin::method_thunk(void* p, x0::FlowArray& args, void* cx)
{
	x0::FlowParams pargs(args.size() - 1, &args[1]);
	(static_cast<T*>(p)->*cb)(static_cast<x0::HttpRequest*>(cx), pargs, args[0]);
}

// main handler
template<typename T, bool (T::*cb)(x0::HttpRequest *, const x0::FlowParams&)>
void XzeroPlugin::registerHandler(const std::string& name)
{
	daemon_->registerHandler(name, &handler_thunk<T, cb>, static_cast<T*>(this));
}

template<typename T, bool (T::*cb)(x0::HttpRequest *, const x0::FlowParams& args)>
void XzeroPlugin::handler_thunk(void *p, x0::FlowArray& args, void *cx)
{
	x0::FlowParams pargs(args.size() - 1, &args[1]);
	args[0].set((static_cast<T*>(p)->*cb)(static_cast<x0::HttpRequest*>(cx), pargs));
}
// }}}

// {{{ inlines
inline XzeroDaemon& XzeroPlugin::daemon() const
{
	return *daemon_;
}

inline x0::HttpServer& XzeroPlugin::server() const
{
	return *server_;
}

/** retrieves the plugin's unique basename (index, userdir, sendfile, auth, etc...) */
inline std::string XzeroPlugin::name() const
{
	return name_;
}

template<typename... Args>
inline void XzeroPlugin::log(x0::Severity sv, const char *msg, Args&&... args)
{
	x0::Buffer fmt;
	fmt.push_back(name_);
	fmt.push_back(": ");
	fmt.push_back(msg);

	server_->log(sv, fmt.c_str(), args...);
}

template<typename... Args>
inline void XzeroPlugin::debug(int level, const char *msg, Args&&... args)
{
#if !defined(XZERO_NDEBUG)
	if (level <= debug_level_)
	{
		x0::Buffer fmt;
		fmt.push_back(name_);
		fmt.push_back(": ");
		fmt.push_back(msg);

		server_->log(x0::Severity::debug, fmt.c_str(), args...);
	}
#endif
}

#if !defined(XZERO_NDEBUG)
inline int XzeroPlugin::debug_level() const
{
	return debug_level_;
}

inline void XzeroPlugin::debug_level(int value)
{
	debug_level_ = value;
}
#endif
// }}}

#define X0_EXPORT_PLUGIN(pluginName) \
	X0_EXPORT_PLUGIN_CLASS(pluginName##_plugin)

#define X0_EXPORT_PLUGIN_CLASS(className) \
	extern "C" X0_EXPORT x0d::XzeroPlugin *x0plugin_init(x0d::XzeroDaemon* d, const std::string& name) { \
		return new className(d, name); \
	}

} // namespace x0d

//@}

#endif
