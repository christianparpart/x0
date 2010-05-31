/* <x0/server.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_http_server_hpp
#define sw_x0_http_server_hpp (1)

#include <x0/http/request_handler.hpp>
#include <x0/http/context.hpp>
#include <x0/io/fileinfo_service.hpp>
#include <x0/settings.hpp>
#include <x0/datetime.hpp>
#include <x0/property.hpp>
#include <x0/library.hpp>
#include <x0/logger.hpp>
#include <x0/signal.hpp>
#include <x0/scope.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>

#include <x0/sysconfig.h>

#include <cstring>
#include <string>
#include <memory>
#include <list>
#include <map>

#include <ev.h>

namespace x0 {

struct plugin;

//! \addtogroup core
//@{

/**
 * \brief implements the x0 web server.
 *
 * \see connection, request, response, plugin
 * \see server::run(), server::stop()
 */
class server :
	public scope
{
	server(const server&) = delete;
	server& operator=(const server&) = delete;

private:
	typedef std::pair<plugin_ptr, library> plugin_value_t;
	typedef std::map<std::string, plugin_value_t> plugin_map_t;

public:
	typedef x0::signal<void(connection *)> connection_hook;
	typedef x0::signal<void(request *)> request_parse_hook;
	typedef x0::signal<void(request *, response *)> request_post_hook;

public:
	explicit server(struct ::ev_loop *loop = 0);
	~server();

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

	// {{{ context management
	x0::scope& create_vhost(const std::string& hostid)
	{
		if (vhosts_.find(hostid) == vhosts_.end())
		{
			vhosts_[hostid] = std::make_shared<x0::scope>(hostid);
		}
		return *vhosts_[hostid];
	}

	void link_vhost(const std::string& master, const std::string& alias)
	{
		vhosts_[alias] = vhosts_[master];
	}

	void unlink_vhost(const std::string& hostid)
	{
		auto i = vhosts_.find(hostid);
		if (i != vhosts_.end())
		{
			vhosts_.erase(i);
		}
	}

	class x0::scope& vhost(const std::string& hostid)
	{
		auto i = vhosts_.find(hostid);
		if (i != vhosts_.end())
			return *i->second;

		return *(vhosts_[hostid] = std::make_shared<x0::scope>(hostid));
	}
	// }}}

	/** 
	 * retrieves reference to server currently loaded configuration.
	 */
	x0::settings& config();

	/**
	 * writes a log entry into the server's error log.
	 */
	void log(severity s, const char *msg, ...);

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args)
	{
#if !defined(NDEBUG)
		if (level >= debug_level_)
			log(severity::debug, msg, args...);
#endif
	}

#if !defined(NDEBUG)
	int debug_level() const;
	void debug_level(int value);
#endif

	/**
	 * sets up a TCP/IP listener on given bind_address and port.
	 *
	 * If there is already a listener on this bind_address:port pair
	 * then no error will be raised.
	 */
	listener *setup_listener(int port, const std::string& bind_address = "0::0");

	/**
	 * loads a plugin into the server.
	 *
	 * \see plugin, unload_plugin(), loaded_plugins()
	 */
	void load_plugin(const std::string& name);

	/** safely unloads a plugin. */
	void unload_plugin(const std::string& name);

	/** retrieves a list of currently loaded plugins */
	std::vector<std::string> loaded_plugins() const;

	struct ::ev_loop *loop() const;

	/** retrieves the current server time. */
	const datetime& now() const;

	const std::list<listener *>& listeners() const;

	bool register_cvar(const std::string& key, context cx, const std::function<bool(const settings_value&, scope&)>& callback, int priority = 0);

private:
	long long getrlimit(int resource);
	long long setrlimit(int resource, long long max);

	listener *listener_by_port(int port);

	bool setup_logging(const settings_value& cvar, scope& s);
	bool setup_resources(const settings_value& cvar, scope& s);
	bool setup_modules(const settings_value& cvar, scope& s);
	bool setup_fileinfo(const settings_value& cvar, scope& s);
	bool setup_error_documents(const settings_value& cvar, scope& s);
	bool setup_hosts(const settings_value& cvar, scope& s);
	bool setup_advertise(const settings_value& cvar, scope& s);

#if defined(WITH_SSL)
	static void gnutls_log(int level, const char *msg);
#endif

	friend class connection;

private:
	void handle_request(request *in, response *out);
	void loop_check(ev::check& w, int revents);

	std::map<std::string, std::shared_ptr<x0::scope>> vhosts_;	//!< virtual host scopes
	std::list<listener *> listeners_;
	struct ::ev_loop *loop_;
	bool active_;
	x0::settings settings_;
	std::map<int, std::map<std::string, std::function<bool(const settings_value&, scope&)>>> cvars_server_;
	std::map<int, std::map<std::string, std::function<bool(const settings_value&, scope&)>>> cvars_host_;
	std::map<int, std::map<std::string, std::function<bool(const settings_value&, scope&)>>> cvars_path_;
	std::string configfile_;
	logger_ptr logger_;
	int debug_level_;
	bool colored_log_;
	plugin_map_t plugins_;
	datetime now_;
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
	fileinfo_service fileinfo;

	property<unsigned long long> max_fds;
};

// {{{ inlines
inline struct ::ev_loop *server::loop() const
{
	return loop_;
}

inline const x0::datetime& server::now() const
{
	return now_;
}

inline const std::list<listener *>& server::listeners() const
{
	return listeners_;
}

#if !defined(NDEBUG)
inline int server::debug_level() const
{
	return debug_level_;
}

inline void server::debug_level(int value)
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
