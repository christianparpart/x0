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
#include <flow/Flow.h>
#include <flow/Value.h>

#include <string>
#include <vector>
#include <system_error>

namespace x0 {

//! \addtogroup core
//@{

class Params
{
private:
	size_t count_;
	const Flow::Value *params_;

public:
	Params() : count_(0), params_(NULL) {}
	Params(int count, Flow::Value *params) :
		count_(count), params_(params) {}

	bool empty() const { return count_ == 0; }
	size_t count() const { return count_; }
	const Flow::Value& at(size_t i) const { return params_[i]; }
	const Flow::Value& operator[](size_t i) const { return params_[i]; }

	bool load(size_t i, bool& out) const;
	bool load(size_t i, long long& out) const;
	bool load(size_t i, std::string& out) const;
	// ...
};

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

	HttpServer& server() const;

protected:
	template<typename T, void (T::*cb)(Flow::Value&, const Params&)> void registerSetupProperty(const std::string& name, Flow::Value::Type resultType);
	template<typename T, void (T::*cb)(Flow::Value&, const Params&)> void registerSetupFunction(const std::string& name, Flow::Value::Type resultType);

	template<typename T, void (T::*cb)(Flow::Value&, HttpRequest *, const Params&)> void registerProperty(const std::string& name, Flow::Value::Type resultType);
	template<typename T, void (T::*cb)(Flow::Value&, HttpRequest *, const Params&)> void registerFunction(const std::string& name, Flow::Value::Type resultType);
	template<typename T, bool (T::*cb)(HttpRequest *, const Params&)> void registerHandler(const std::string& name);

protected:
	HttpServer& server_;
	std::string name_;

#if !defined(NDEBUG)
	int debug_level_;
#endif 
private:
	template<class T, void (T::*cb)(Flow::Value&, const Params&)> static void setup_thunk(void *p, int argc, Flow::Value *argv, void *cx);
	template<class T, void (T::*cb)(Flow::Value&, HttpRequest *, const Params&)> static void method_thunk(void *p, int argc, Flow::Value *argv, void *cx);
	template<class T, bool (T::*cb)(HttpRequest *, const Params&)> static void handler_thunk(void *p, int argc, Flow::Value *argv, void *cx);

	friend class HttpServer;
};

// {{{ flow integration
// setup property
template<typename T, void (T::*cb)(Flow::Value&, const Params&)>
void HttpPlugin::registerSetupProperty(const std::string& name, Flow::Value::Type resultType)
{
	server_.registerVariable(name, resultType, &setup_thunk<T, cb>, static_cast<T *>(this));
}

// setup function
template<typename T, void (T::*cb)(Flow::Value&, const Params&)>
void HttpPlugin::registerSetupFunction(const std::string& name, Flow::Value::Type resultType)
{
	server_.registerFunction(name, resultType, &setup_thunk<T, cb>, static_cast<T *>(this));
}

template<class T, void (T::*cb)(Flow::Value&, const Params&)>
void HttpPlugin::setup_thunk(void *p, int argc, Flow::Value *argv, void * /*cx*/)
{
	Params args(argc, argv + 1);
	(static_cast<T *>(p)->*cb)(argv[0], args);
}

// property
template<typename T, void (T::*cb)(Flow::Value&, HttpRequest *, const Params&)>
void HttpPlugin::registerProperty(const std::string& name, Flow::Value::Type resultType)
{
	server_.registerVariable(name, resultType, &method_thunk<T, cb>, static_cast<T *>(this));
}

// methods
template<typename T, void (T::*cb)(Flow::Value&, HttpRequest *, const Params&)>
void HttpPlugin::registerFunction(const std::string& name, Flow::Value::Type resultType)
{
	server_.registerFunction(name, resultType, &method_thunk<T, cb>, static_cast<T *>(this));
}

template<typename T, void (T::*cb)(Flow::Value&, HttpRequest *, const Params&)>
void HttpPlugin::method_thunk(void *p, int argc, Flow::Value *argv, void *cx)
{
	T *self = static_cast<T *>(p);
	HttpRequest *r = static_cast<HttpRequest  *>(cx);
	Params args(argc, argv + 1);

	(self->*cb)(argv[0], r, args);
}

// handler
template<typename T, bool (T::*cb)(HttpRequest *, const Params&)>
void HttpPlugin::registerHandler(const std::string& name)
{
	server_.registerHandler(name, &handler_thunk<T, cb>, static_cast<T *>(this));
}

template<typename T, bool (T::*cb)(HttpRequest *, const Params& args)>
void HttpPlugin::handler_thunk(void *p, int argc, Flow::Value *argv, void *cx)
{
	T *self = static_cast<T *>(p);
	HttpRequest *r = static_cast<HttpRequest *>(cx);
	Params args(argc, argv + 1);

	argv[0].set((self->*cb)(r, args));
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
	X0_EXPORT_PLUGIN_CLASS(pluginName, pluginName##_plugin)

#define X0_EXPORT_PLUGIN_CLASS(pluginName, className) \
	extern "C" X0_EXPORT x0::HttpPlugin *x0plugin_init(x0::HttpServer& srv, const std::string& name) { \
		return new className(srv, name); \
	}

//@}

} // namespace x0

#endif
