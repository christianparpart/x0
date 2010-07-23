/* <HttpServer.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_http_server_h
#define sw_x0_http_server_h (1)

#include <x0/http/HttpContext.h>
#include <x0/http/HttpRequestHandler.h>
#include <x0/http/Types.h>
#include <x0/io/FileInfoService.h>
#include <x0/Settings.h>
#include <x0/DateTime.h>
#include <x0/Property.h>
#include <x0/Library.h>
#include <x0/Logger.h>
#include <x0/Signal.h>
#include <x0/Scope.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <x0/sysconfig.h>

#include <cstring>
#include <string>
#include <memory>
#include <list>
#include <map>

#include <ev.h>

namespace x0 {

struct HttpPlugin;
struct HttpCore;

//! \addtogroup core
//@{

/**
 * \brief implements the x0 web server.
 *
 * \see HttpConnection, HttpRequest, HttpResponse, HttpPlugin
 * \see HttpServer::run(), HttpServer::stop()
 */
class HttpServer :
	public Scope
{
	HttpServer(const HttpServer&) = delete;
	HttpServer& operator=(const HttpServer&) = delete;

public:
	typedef Signal<void(HttpConnection *)> ConnectionHook;
	typedef Signal<void(HttpRequest *)> RequestHook;
	typedef Signal<void(HttpRequest *, HttpResponse *)> RequestPostHook;

public:
	explicit HttpServer(struct ::ev_loop *loop = 0);
	~HttpServer();

	void setLogger(std::shared_ptr<Logger> logger);
	Logger *logger() const;

	// {{{ service control
	bool configure(const std::string& configfile);
	bool start();
	bool active() const;
	void run();
	void pause();
	void resume();
	void reload();
	void stop();
	// }}}

	// {{{ signals raised on request in order
	ConnectionHook onConnectionOpen;	//!< This hook is invoked once a new client has connected.
	RequestHook onPreProcess; 			//!< is called at the very beginning of a request.
	RequestHook onResolveDocumentRoot;	//!< resolves document_root to use for this request.
	RequestHook onResolveEntity;		//!< maps the request URI into local physical path.
	HttpRequestHandler onHandleRequest;	//!< generates response content for this request being processed.
	RequestPostHook onPostProcess;		//!< gets invoked right before serializing headers
	RequestPostHook onRequestDone;		//!< this hook is invoked once the request has been <b>fully</b> served to the client.
	ConnectionHook onConnectionClose;	//!< is called before a connection gets closed / or has been closed by remote point.
	// }}}

	// {{{ virtual-host management
	Scope *createHost(const std::string& hostid);
	Scope *createHostAlias(const std::string& master, const std::string& alias);
	void removeHostAlias(const std::string& hostid);
	void removeHost(const std::string& hostid);

	Scope *resolveHost(const std::string& hostid) const;
	std::list<Scope *> getHostsByPort(int port) const;

	std::vector<std::string> hostnames() const; //!< retrieves a list of host names (w/o aliases)
	std::vector<std::string> allHostnames() const; //!< retrieves a list of host names and their aliases
	std::vector<std::string> hostnamesOf(const std::string& master) const; //!< retrieves all host names for a given virtual-host ID
	// }}}

	/** 
	 * retrieves reference to server currently loaded configuration.
	 */
	Settings& config();

	void addComponent(const std::string& value);

	/**
	 * writes a log entry into the server's error log.
	 */
	void log(Severity s, const char *msg, ...);

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args)
	{
#if !defined(NDEBUG)
		if (level >= debug_level_)
			log(Severity::debug, msg, args...);
#endif
	}

#if !defined(NDEBUG)
	int debugLevel() const;
	void debugLevel(int value);
#endif

	HttpListener *setupListener(int port, const std::string& bindAddress);

	std::string pluginDirectory() const;
	void setPluginDirectory(const std::string& value);

	HttpPlugin *loadPlugin(const std::string& name, std::error_code& ec);
	template<typename T> T *loadPlugin(const std::string& name, std::error_code& ec);
	void unloadPlugin(const std::string& name);
	std::vector<std::string> pluginsLoaded() const;

	HttpPlugin *registerPlugin(HttpPlugin *plugin);
	HttpPlugin *unregisterPlugin(HttpPlugin *plugin);

	struct ::ev_loop *loop() const;

	/** retrieves the current server time. */
	const DateTime& now() const;

	HttpCore& core() const;

	const std::list<HttpListener *>& listeners() const;

	bool declareCVar(const std::string& key, HttpContext cx, const cvar_handler& callback, int priority = 0);
	std::vector<std::string> cvars(HttpContext cx) const;
	void undeclareCVar(const std::string& key);

	HttpListener *listenerByHost(const std::string& hostid) const;
	HttpListener *listenerByPort(int port) const;

private:
#if defined(WITH_SSL)
	static void gnutls_log(int level, const char *msg);
#endif

	friend class HttpConnection;
	friend class HttpCore;

private:
	void handle_request(HttpRequest *in, HttpResponse *out);
	void loop_check(ev::check& w, int revents);

	std::vector<std::string> components_;
	std::map<std::string, std::shared_ptr<Scope>> vhosts_;	//!< virtual host scopes

	std::list<HttpListener *> listeners_;
	struct ::ev_loop *loop_;
	bool active_;
	Settings settings_;
	std::map<int, std::map<std::string, cvar_handler>> cvars_server_;	//!< registered server-scope cvars
	std::map<int, std::map<std::string, cvar_handler>> cvars_host_;	//!< registered host-scope cvars
	std::map<int, std::map<std::string, cvar_handler>> cvars_path_;	//!< registered location-scope cvars
	std::string configfile_;
	LoggerPtr logger_;
	int debug_level_;
	bool colored_log_;
	std::string pluginDirectory_;
	std::vector<HttpPlugin *> plugins_;
	std::map<HttpPlugin *, Library> pluginLibraries_;
	DateTime now_;
	ev::check loop_check_;
	HttpCore *core_;

public:
	value_property<int> max_connections;
	value_property<int> max_keep_alive_idle;
	value_property<int> max_read_idle;
	value_property<int> max_write_idle;
	value_property<bool> tcp_cork;
	value_property<bool> tcp_nodelay;
	value_property<std::string> tag;
	value_property<bool> advertise;
	FileInfoService fileinfo;
};

// {{{ inlines
inline void HttpServer::setLogger(std::shared_ptr<Logger> logger)
{
	logger_ = logger;
}

inline Logger *HttpServer::logger() const
{
	return logger_.get();
}

inline struct ::ev_loop *HttpServer::loop() const
{
	return loop_;
}

inline const DateTime& HttpServer::now() const
{
	return now_;
}

inline HttpCore& HttpServer::core() const
{
	return *core_;
}

inline const std::list<HttpListener *>& HttpServer::listeners() const
{
	return listeners_;
}

template<typename T>
inline T *HttpServer::loadPlugin(const std::string& name, std::error_code& ec)
{
	return dynamic_cast<T *>(loadPlugin(name, ec));
}

/** retrieves scope of given virtual host or global scope if not found.
 *
 * \param hostid the virtual hosts id (hostname ':' port)
 *
 * \return the virtual hosts scope (if found) or global (server) scope if not found.
 */
inline Scope *HttpServer::resolveHost(const std::string& hostid) const
{
	auto i = vhosts_.find(hostid);
	if (i != vhosts_.end())
		return i->second.get();

	return const_cast<HttpServer *>(this);
}

#if !defined(NDEBUG)
inline int HttpServer::debugLevel() const
{
	return debug_level_;
}

inline void HttpServer::debugLevel(int value)
{
	if (value < 0)
		value = 0;
	else if (value > 9)
		value = 9;

	debug_level_ = value;
}
#endif
// }}}

//@}

} // namespace x0

#endif
// vim:syntax=cpp
