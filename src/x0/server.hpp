/* <x0/server.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_server_hpp
#define sw_x0_server_hpp (1)

#include <x0/datetime.hpp>
#include <x0/settings.hpp>
#include <x0/logger.hpp>
#include <x0/listener.hpp>
#include <x0/handler.hpp>
#include <x0/signal.hpp>
#include <x0/event_handler.hpp>
#include <x0/context.hpp>
#include <x0/plugin.hpp>
#include <x0/types.hpp>
#include <x0/property.hpp>
#include <x0/fileinfo_service.hpp>
#include <x0/api.hpp>
#include <boost/signals.hpp>
#include <asio.hpp>
#include <cstring>
#include <string>
#include <list>
#include <map>

namespace x0 {

/**
 * \ingroup core
 * \brief exception raised when given virtual host has not been found.
 */
class host_not_found
	: public std::runtime_error
{
public:
	host_not_found(const std::string& hostname)
		: std::runtime_error("host not found: " + hostname)
	{
	}
};

/**
 * \ingroup core
 * \brief implements the x0 web server.
 *
 * \see connection, request, response, plugin
 * \see server::run(), server::stop()
 */
class server :
	public boost::noncopyable
{
private:
	typedef std::pair<plugin_ptr, void *> plugin_value_t;
	typedef std::map<std::string, plugin_value_t> plugin_map_t;

public:
	server();
	~server();

	// {{{ service control
	/** configures this server as defined in the configuration section(s). */
	void configure(const std::string& configfile);
	/** starts this server object by listening on new connections and processing them. */
	void run();
	/** pauses an already active server by not accepting further new connections until resumed. */
	void pause();
	/** resumes a currently paused server by continueing processing new connections. */
	void resume();
	/** reloads server configuration */
	void reload();
	/** gracefully stops a running server */
	void stop();
	// }}}

	// {{{ signals raised on request in order
	/** is invoked once a new client connection is established */
	event_handler<void(connection_ptr)> connection_open;

	/** is called at the very beginning of a request. */
	x0::signal<void(request&)> pre_process;

	/** resolves document_root to use for this request. */
	x0::signal<void(request&)> resolve_document_root;

	/** resolves request's physical filename (maps URI to physical path). */
	x0::signal<void(request&)> resolve_entity;

	/** generates response content for this request being processed. */
	handler generate_content;

	/** hook for generating accesslog logs and other things to be done after the request has been served. */
	x0::signal<void(request&, response&)> request_done;

	/** is called at the very end of a request. */
	x0::signal<void(request&, response&)> post_process;

	/** is called before a connection gets closed / or has been closed by remote point. */
	x0::signal<void(connection *)> connection_close;
	// }}}

	// {{{ context management
	/** create server context data for given plugin. */
	template<typename T>
	T& create_context(plugin *plug)
	{
		context_.set(plug, new T);
		return context_.get<T>(plug);
	}

	/** creates a virtual-host context for given plugin. */
	template<typename T>
	T& create_context(plugin *plug, const std::string& vhost)
	{
		auto i = vhosts_.find(vhost);
		if (i == vhosts_.end())
			vhosts_[vhost] = std::shared_ptr<x0::context>(new x0::context);

		vhosts_[vhost]->set(plug, new T);

		return vhosts_[vhost]->get<T>(plug);
	}

	void link_context(const std::string& master, const std::string& alias)
	{
		vhosts_[alias] = vhosts_[master];
	}

	/** retrieve the server configuration context. */
	x0::context& context()
	{
		return context_;
	}

	/** retrieve server context data for given plugin
	 * \param plug plugin data for which we want to retrieve the data for.
	 */
	template<typename T>
	T& context(plugin *plug)
	{
		return context_.get<T>(plug);
	}

	/** retrieve virtual-host context data for given plugin
	 * \param plug plugin data for which we want to retrieve the data for.
	 */
	template<typename T>
	T& context(plugin *plug, const std::string& vhostname)
	{
		auto vhost = vhosts_.find(vhostname);

		if (vhost == vhosts_.end())
			throw host_not_found(vhostname);

		auto ctx = *vhost->second;
		auto data = ctx.find(plug);
		if (data != ctx.end())
			return *static_cast<T *>(data->second);

		ctx.set<T>(plug, new T);
		return ctx.get<T>(plug);
	}

	template<typename T>
	T *free_context(plugin *plug)
	{
		/// \todo free all contexts owned by given plugin
		return context_.free<T>(plug);
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

	/**
	 * sets up a TCP/IP listener on given bind_address and port.
	 *
	 * If there is already a listener on this bind_address:port pair
	 * then no error will be raised.
	 */
	void setup_listener(int port, const std::string& bind_address = "0::0");

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

	void handle_request(request& in, response& out);

	asio::io_service& io_service();

	/** retrieves the current server time. */
	const datetime& now() const;

private:
	long long getrlimit(int resource);
	long long setrlimit(int resource, long long max);

	void drop_privileges(const std::string& user, const std::string& group);

	listener_ptr listener_by_port(int port);

private:
	x0::context context_;											//!< server context
	std::map<std::string, std::shared_ptr<x0::context>> vhosts_;	//!< vhost contexts
	std::list<listener_ptr> listeners_;
	asio::io_service io_service_;
	bool paused_;
	x0::settings settings_;
	std::string configfile_;
	logger_ptr logger_;
	plugin_map_t plugins_;
	datetime now_;

public:
	value_property<int> max_connections;
	value_property<int> max_keep_alive_requests;
	value_property<int> max_keep_alive_idle;
	value_property<int> max_read_idle;
	value_property<int> max_write_idle;
	value_property<std::string> tag;
	fileinfo_service fileinfo;

	property<unsigned long long> max_fds;
};

// {{{ inlines
inline asio::io_service& server::io_service()
{
	return io_service_;
}

inline const x0::datetime& server::now() const
{
	return now_;
}
// }}}

} // namespace x0

#endif
// vim:syntax=cpp
