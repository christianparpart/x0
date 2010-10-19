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
#include <x0/http/Types.h>
#include <x0/io/FileInfoService.h>
#include <x0/DateTime.h>
#include <x0/Property.h>
#include <x0/Library.h>
#include <x0/Logger.h>
#include <x0/Signal.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <x0/sysconfig.h>

#include <flow/Backend.h>
#include <flow/Runner.h>

#include <cstring>
#include <string>
#include <memory>
#include <list>
#include <map>

#include <ev.h>

class x0d; // friend declared in HttpServer

namespace x0 {

struct HttpPlugin;
struct HttpCore;
struct HttpWorker;

//! \addtogroup core
//@{

/**
 * \brief implements the x0 web server.
 *
 * \see HttpConnection, HttpRequest, HttpResponse, HttpPlugin
 * \see HttpServer::run(), HttpServer::stop()
 */
class HttpServer :
	public Flow::Backend
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

	HttpWorker *spawnWorker();
	void destroyWorker(HttpWorker *worker);

	// {{{ service control
	bool setup(std::istream *settings);
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
	RequestPostHook onPostProcess;		//!< gets invoked right before serializing headers
	RequestPostHook onRequestDone;		//!< this hook is invoked once the request has been <b>fully</b> served to the client.
	ConnectionHook onConnectionClose;	//!< is called before a connection gets closed / or has been closed by remote point.
	// }}}

	void addComponent(const std::string& value);

	/**
	 * writes a log entry into the server's error log.
	 */
	void log(Severity s, const char *msg, ...);

	template<typename... Args>
	inline void debug(int level, const char *msg, Args&&... args)
	{
#if !defined(NDEBUG)
		if (level <= logLevel_)
			log(Severity(level), msg, args...);
#endif
	}

	Severity logLevel() const;
	void logLevel(Severity value);

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

	HttpListener *listenerByHost(const std::string& hostid) const;
	HttpListener *listenerByPort(int port) const;

	void dumpIR() const; // for debugging purpose

private: // {{{ flow backend API
	virtual void import(const std::string& name, const std::string& path);

	// flow core API (setup stage)
	static void flow_plugin_directory(void *p, int argc, Flow::Value *argv);
	static void flow_plugin_load(void *p, int argc, Flow::Value *argv);
	static void flow_mimetypes(void *p, int argc, Flow::Value *argv);
	static void flow_listen(void *p, int argc, Flow::Value *argv);
	static void flow_group(void *p, int argc, Flow::Value *argv);
	static void flow_user(void *p, int argc, Flow::Value *argv);

	// flow core API (general)
	static void flow_log(void *p, int argc, Flow::Value *argv);
	bool printValue(const Flow::Value& value);
	static void flow_sys_env(void *p, int argc, Flow::Value *argv);
	static void flow_sys_cwd(void *p, int argc, Flow::Value *argv);
	static void flow_sys_pid(void *p, int argc, Flow::Value *argv);
	static void flow_sys_now(void *p, int argc, Flow::Value *argv);
	static void flow_sys_now_str(void *p, int argc, Flow::Value *argv);

	// flow API: (main)
	// (request)
	static void flow_req_docroot(void *p, int argc, Flow::Value *argv);
	static void flow_req_method(void *p, int argc, Flow::Value *argv);
	static void flow_req_url(void *p, int argc, Flow::Value *argv);
	static void flow_req_path(void *p, int argc, Flow::Value *argv);
	static void flow_req_header(void *p, int argc, Flow::Value *argv);
	static void flow_hostname(void *p, int argc, Flow::Value *argv);

	// (response)
	static void flow_redirect(void *p, int argc, Flow::Value *argv);
	static void flow_respond(void *p, int argc, Flow::Value *argv);
	static void flow_header_add(void *p, int argc, Flow::Value *argv);
	static void flow_header_append(void *p, int argc, Flow::Value *argv);
	static void flow_header_overwrite(void *p, int argc, Flow::Value *argv);
	static void flow_header_remove(void *p, int argc, Flow::Value *argv);

	// (connection)
	static void flow_remote_ip(void *p, int argc, Flow::Value *argv);
	static void flow_remote_port(void *p, int argc, Flow::Value *argv);
	static void flow_local_ip(void *p, int argc, Flow::Value *argv);
	static void flow_local_port(void *p, int argc, Flow::Value *argv);

	// (physical file)
	static void flow_phys_exists(void *p, int argc, Flow::Value *argv);
	static void flow_phys_is_dir(void *p, int argc, Flow::Value *argv);
	static void flow_phys_is_reg(void *p, int argc, Flow::Value *argv);
	static void flow_phys_is_exe(void *p, int argc, Flow::Value *argv);
	static void flow_phys_mtime(void *p, int argc, Flow::Value *argv);
	static void flow_phys_size(void *p, int argc, Flow::Value *argv);
	static void flow_phys_etag(void *p, int argc, Flow::Value *argv);
	static void flow_phys_mimetype(void *p, int argc, Flow::Value *argv);
	// }}}

private:
#if defined(WITH_SSL)
	static void gnutls_log(int level, const char *msg);
#endif

	friend class HttpConnection;
	friend class HttpPlugin;
	friend class HttpCore;
	friend class ::x0d; // to access in_ and out_.

private:
	void handleRequest(HttpRequest *in, HttpResponse *out);
	void loop_check(ev::check& w, int revents);

	static void *runWorker(void *);

	std::vector<std::string> components_;

	Flow::Unit *unit_;
	Flow::Runner *runner_;
	bool (*onHandleRequest_)();
	HttpRequest *in_;
	HttpResponse *out_;

	std::list<HttpListener *> listeners_;
	struct ::ev_loop *loop_;
	bool active_;
	LoggerPtr logger_;
	Severity logLevel_;
	bool colored_log_;
	std::string pluginDirectory_;
	std::vector<HttpPlugin *> plugins_;
	std::map<HttpPlugin *, Library> pluginLibraries_;
	DateTime now_;
	ev::check loop_check_;
	HttpCore *core_;
	std::list<HttpWorker *> workers_;

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

inline Severity HttpServer::logLevel() const
{
	return logLevel_;
}

inline void HttpServer::logLevel(Severity value)
{
	logLevel_ = value;
	logger()->level(value);
}
// }}}

//@}

} // namespace x0

#endif
// vim:syntax=cpp
