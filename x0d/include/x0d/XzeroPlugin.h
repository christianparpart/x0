/* <XzeroPlugin.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0d/XzeroDaemon.h>
#include <x0/http/HttpRequest.h>
#include <x0/flow/AST.h>
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
    template<typename Class, typename... ArgTypes> x0::FlowVM::NativeCallback& setupFunction(const std::string& name, void (Class::*method)(x0::FlowVM::Params&), ArgTypes... argTypes);
    template<typename Class, typename... ArgTypes> x0::FlowVM::NativeCallback& sharedFunction(const std::string& name, void (Class::*method)(x0::HttpRequest* r, x0::FlowVM::Params&), ArgTypes... argTypes);
    template<typename Class, typename... ArgTypes> x0::FlowVM::NativeCallback& mainFunction(const std::string& name, void (Class::*method)(x0::HttpRequest* r, x0::FlowVM::Params&), ArgTypes... argTypes);
    template<typename Class, typename... ArgTypes> x0::FlowVM::NativeCallback& mainHandler(const std::string& name, bool (Class::*method)(x0::HttpRequest* r, x0::FlowVM::Params&), ArgTypes... argTypes);

protected:
	XzeroDaemon* daemon_;
	x0::HttpServer* server_;
	std::string name_;

#if !defined(XZERO_NDEBUG)
	int debugLevel_;
#endif 

	friend class XzeroDaemon;
};

// {{{ flow integration
template<typename Class, typename... ArgTypes> inline x0::FlowVM::NativeCallback& XzeroPlugin::setupFunction(const std::string& name, void (Class::*method)(x0::FlowVM::Params&), ArgTypes... argTypes)
{
    return daemon_->setupFunction(name, [=](x0::FlowVM::Params& args) { (((Class*)this)->*method)(args); }, argTypes...);
}

template<typename Class, typename... ArgTypes> inline x0::FlowVM::NativeCallback& XzeroPlugin::sharedFunction(const std::string& name, void (Class::*method)(x0::HttpRequest*, x0::FlowVM::Params&), ArgTypes... argTypes)
{
    return daemon_->sharedFunction(name, [=](x0::FlowVM::Params& args) { (((Class*)this)->*method)((x0::HttpRequest*) args.caller()->userdata(), args); }, argTypes...);
}

template<typename Class, typename... ArgTypes> inline x0::FlowVM::NativeCallback& XzeroPlugin::mainFunction(const std::string& name, void (Class::*method)(x0::HttpRequest*, x0::FlowVM::Params&), ArgTypes... argTypes)
{
    return daemon_->mainFunction(name, [=](x0::FlowVM::Params& args) { (((Class*)this)->*method)((x0::HttpRequest*) args.caller()->userdata(), args); }, argTypes...);
}

template<typename Class, typename... ArgTypes>
inline x0::FlowVM::NativeCallback& XzeroPlugin::mainHandler(
        const std::string& name, bool (Class::*method)(x0::HttpRequest*, x0::FlowVM::Params&), ArgTypes... argTypes)
{
    return daemon_->mainHandler(name, [=](x0::FlowVM::Params& args) {
        args.setResult((((Class*)this)->*method)((x0::HttpRequest*) args.caller()->userdata(), args));
    }, argTypes...);
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
	if (level <= debugLevel_)
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
	return debugLevel_;
}

inline void XzeroPlugin::debug_level(int value)
{
	debugLevel_ = value;
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
