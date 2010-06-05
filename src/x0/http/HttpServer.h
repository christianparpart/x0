/* <x0/HttpServer.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_http_server_h
#define sw_x0_http_server_h (1)

#include <x0/http/request_handler.h>
#include <x0/http/HttpContext.h>
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

private:
	typedef std::pair<HttpPluginPtr, Library> plugin_value_t;
	typedef std::map<std::string, plugin_value_t> plugin_map_t;

public:
	typedef x0::signal<void(HttpConnection *)> connection_hook;
	typedef x0::signal<void(HttpRequest *)> request_parse_hook;
	typedef x0::signal<void(HttpRequest *, HttpResponse *)> request_post_hook;

public:
	explicit HttpServer(struct ::ev_loop *loop = 0);
	~HttpServer();

	// {{{ service control
	void configure(const std::string& configfile);
	void start();
	bool active() const;
	void run();
	void pause();
	void resume();
	void reload();
	void stop();
	// }}}

	// {{{ signals raised on request in order
	connection_hook connection_open;		//!< This hook is invoked once a new client has connected.
	request_parse_hook pre_process; 		//!< is called at the very beginning of a request.
	request_parse_hook resolve_document_root;//!< resolves document_root to use for this request.
	request_parse_hook resolve_entity;		//!< maps the request URI into local physical path.
	request_handler generate_content;		//!< generates response content for this request being processed.
	request_post_hook post_process;			//!< gets invoked right before serializing headers
	request_post_hook request_done;			//!< this hook is invoked once the request has been <b>fully</b> served to the client.
	connection_hook connection_close;		//!< is called before a connection gets closed / or has been closed by remote point.
	// }}}

	// {{{ HttpContext management
	x0::Scope& createHost(const std::string& hostid)
	{
		if (vhosts_.find(hostid) == vhosts_.end())
		{
			vhosts_[hostid] = std::make_shared<x0::Scope>(hostid);
		}
		return *vhosts_[hostid];
	}

	void linkHost(const std::string& master, const std::string& alias)
	{
		vhosts_[alias] = vhosts_[master];
	}

	void unlinkHost(const std::string& hostid)
	{
		auto i = vhosts_.find(hostid);
		if (i != vhosts_.end())
		{
			vhosts_.erase(i);
		}
	}

	class x0::Scope& host(const std::string& hostid)
	{
		auto i = vhosts_.find(hostid);
		if (i != vhosts_.end())
			return *i->second;

		return *(vhosts_[hostid] = std::make_shared<x0::Scope>(hostid));
	}
	// }}}

	/** 
	 * retrieves reference to server currently loaded configuration.
	 */
	Settings& config();

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

	/**
	 * sets up a TCP/IP HttpListener on given bind_address and port.
	 *
	 * If there is already a HttpListener on this bind_address:port pair
	 * then no error will be raised.
	 */
	HttpListener *setupListener(int port, const std::string& bind_address = "0::0");

	/**
	 * loads a plugin into the server.
	 *
	 * \see plugin, unload_plugin(), loaded_plugins()
	 */
	void loadPlugin(const std::string& name);

	/** safely unloads a plugin. */
	void unloadPlugin(const std::string& name);

	/** retrieves a list of currently loaded plugins */
	std::vector<std::string> pluginsLoaded() const;

	struct ::ev_loop *loop() const;

	/** retrieves the current server time. */
	const DateTime& now() const;

	const std::list<HttpListener *>& listeners() const;

	bool declareCVar(const std::string& key, HttpContext cx, const cvar_handler& callback, int priority = 0);
	std::vector<std::string> cvars(HttpContext cx) const;
	void undeclareCVar(const std::string& key);

private:
	long long getrlimit(int resource);
	long long setrlimit(int resource, long long max);

	HttpListener *listener_by_port(int port);

	bool setup_logging(const SettingsValue& cvar, Scope& s);
	bool setup_resources(const SettingsValue& cvar, Scope& s);
	bool setup_modules(const SettingsValue& cvar, Scope& s);
	bool setup_fileinfo(const SettingsValue& cvar, Scope& s);
	bool setup_error_documents(const SettingsValue& cvar, Scope& s);
	bool setup_hosts(const SettingsValue& cvar, Scope& s);
	bool setup_advertise(const SettingsValue& cvar, Scope& s);

#if defined(WITH_SSL)
	static void gnutls_log(int level, const char *msg);
#endif

	friend class HttpConnection;

private:
	void handle_request(HttpRequest *in, HttpResponse *out);
	void loop_check(ev::check& w, int revents);

	std::map<std::string, std::shared_ptr<x0::Scope>> vhosts_;	//!< virtual host scopes
	std::list<HttpListener *> listeners_;
	struct ::ev_loop *loop_;
	bool active_;
	Settings settings_;
	std::map<int, std::map<std::string, std::function<bool(const SettingsValue&, Scope&)>>> cvars_server_;
	std::map<int, std::map<std::string, std::function<bool(const SettingsValue&, Scope&)>>> cvars_host_;
	std::map<int, std::map<std::string, std::function<bool(const SettingsValue&, Scope&)>>> cvars_path_;
	std::string configfile_;
	LoggerPtr logger_;
	int debug_level_;
	bool colored_log_;
	plugin_map_t plugins_;
	DateTime now_;
	ev::check loop_check_;

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

	property<unsigned long long> max_fds;
};

// {{{ inlines
inline struct ::ev_loop *HttpServer::loop() const
{
	return loop_;
}

inline const x0::DateTime& HttpServer::now() const
{
	return now_;
}

inline const std::list<HttpListener *>& HttpServer::listeners() const
{
	return listeners_;
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
