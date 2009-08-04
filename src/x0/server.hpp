/* <x0/server.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_server_hpp
#define sw_x0_server_hpp (1)

#include <x0/config.hpp>
#include <x0/logger.hpp>
#include <x0/listener.hpp>
#include <x0/io_service_pool.hpp>
#include <x0/handler.hpp>
#include <x0/context.hpp>
#include <x0/plugin.hpp>
#include <x0/types.hpp>
#include <x0/property.hpp>
#include <x0/stat_service.hpp>
#include <x0/api.hpp>
#include <boost/signals.hpp>
#include <cstring>
#include <string>
#include <list>
#include <map>

namespace x0 {

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
	boost::signal<void(connection_ptr)> connection_open;

	/** is called at the very beginning of a request. */
	boost::signal<void(request&)> pre_process;

	/** resolves document_root to use for this request. */
	boost::signal<void(request&)> resolve_document_root;

	/** resolves request's physical filename (maps URI to physical path). */
	boost::signal<void(request&)> resolve_entity;

	/** generates response content for this request being processed. */
	handler generate_content;

	/** hook for generating accesslog logs and other things to be done after the request has been served. */
	boost::signal<void(request&, response&)> request_done;

	/** is called at the very end of a request. */
	boost::signal<void(request&, response&)> post_process;

	/** is called before a connection gets closed / or has been closed by remote point. */
	boost::signal<void(connection *)> connection_close;
	// }}}

	// {{{ configuration contexts
	/** create server context data for given plugin. */
	template<typename T>
	T& create_context(plugin *plug, T *d)
	{
		context_.set(plug, d);
		return context_.get<T>(plug);
	}

	/** retrieve server context data for given plugin. */
	template<typename T>
	T& context(plugin *plug)
	{
		return context<T>(plug, "/");
	}

	/** retrieve context data for given plugin at mapped file system path.
	 * \param plug plugin data for which we want to retrieve the data for.
	 * \param path mapped local file system path this context corresponds to. (e.g. "/var/www/")
	 */
	template<typename T>
	T& context(plugin *plug, const std::string& path)
	{
		return context_.get<T>(plug);
	}

	template<typename T>
	T *free_context(plugin *plug)
	{
		return context_.free<T>(plug);
	}

	/** retrieve the server configuration context. */
	x0::context& context()
	{
		return context_;
	}
	// }}}

	/** 
	 * retrieves reference to server currently loaded configuration.
	 */
	x0::config& config();

	/**
	 * writes a log entry into the server's error log.
	 */
	void log(const char *filename, unsigned int line, severity s, const char *msg, ...);

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

	x0::io_service_pool& io_service_pool();

private:
	void setrlimit(int resource, unsigned long long max);
	void drop_privileges(const std::string& user, const std::string& group);

	listener_ptr listener_by_port(int port);

private:
	x0::context context_;
	std::list<listener_ptr> listeners_;
	x0::io_service_pool io_service_pool_;
	bool paused_;
	x0::config config_;
	std::string configfile_;
	logger_ptr logger_;
	plugin_map_t plugins_;

public:
	value_property<int> max_connections;
	value_property<int> max_fds;
	value_property<int> max_keep_alive_requests;
	value_property<int> max_keep_alive_idle;
	value_property<int> max_read_idle;
	value_property<int> max_write_idle;
	value_property<std::string> tag;
	stat_service stat;
};

#define LOG(srv, severity, message...) (srv).log(__FILENAME__, __LINE__, severity, message)

// {{{ inlines
inline x0::io_service_pool& server::io_service_pool()
{
	return io_service_pool_;
}
// }}}

} // namespace x0

#endif
// vim:syntax=cpp
