/* <HttpServer.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_http_server_h
#define sw_x0_http_server_h (1)

#include <x0/io/FileInfoService.h>
#include <x0/http/HttpConnection.h> // HttpConnection, HttpConnection::Status
#include <x0/http/Types.h>
#include <x0/ServerSocket.h>
#include <x0/LogMessage.h>
#include <x0/TimeSpan.h>
#include <x0/Library.h>
#include <x0/Logger.h>
#include <x0/Signal.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <x0/sysconfig.h>

#include <x0/flow/FlowBackend.h>
#include <x0/flow/FlowRunner.h>

#include <cstring>
#include <string>
#include <memory>
#include <list>
#include <unordered_map>
#include <atomic>

#include <ev++.h>

class x0d; // friend declared in HttpServer

namespace x0 {

class SocketSpec;

//! \addtogroup http
//@{

class HttpPlugin;
class HttpCore;
class HttpWorker;

/**
 * \brief implements the x0 web server.
 *
 * \see HttpConnection, HttpRequest, HttpPlugin
 * \see HttpServer::run(), HttpServer::stop()
 */
class X0_API HttpServer :
	public FlowBackend
{
	HttpServer(const HttpServer&) = delete;
	HttpServer& operator=(const HttpServer&) = delete;

public:
	typedef Signal<void(HttpConnection*)> ConnectionHook;
	typedef Signal<void(HttpConnection*, HttpConnection::Status)> ConnectionStatusHook;
	typedef Signal<void(HttpRequest*)> RequestHook;
	typedef Signal<void(HttpWorker*)> WorkerHook;

public:
	std::function<bool(HttpRequest*)> requestHandler;

	explicit HttpServer(struct ::ev_loop* loop, unsigned generation = 1);
	virtual ~HttpServer();

	void setLogger(std::shared_ptr<Logger> logger);
	Logger* logger() const;

	void cycleLogs();

	ev_tstamp startupTime() const { return startupTime_; }
	ev_tstamp uptime() const { return ev_now(loop_) - startupTime_; }

	HttpWorker* nextWorker();
	HttpWorker* spawnWorker();
	HttpWorker* selectWorker();
	HttpWorker* mainWorker() const { return workers_[0]; }
	const std::vector<HttpWorker*>& workers() const { return workers_; }
	void destroyWorker(HttpWorker* worker);

	// {{{ service control
	bool setup(std::istream* settings, const std::string& filename = std::string(), int optimizationLevel = 2);
	bool setup(const std::string& filename, int optimizationLevel = 2);
	int run();
	void stop();
	void kill();
	// }}}

	// {{{ signals raised on request in order
	ConnectionHook onConnectionOpen;	//!< This hook is invoked once a new client has connected.
	RequestHook onPreProcess; 			//!< is called at the very beginning of a request.
	RequestHook onResolveDocumentRoot;	//!< resolves document_root to use for this request.
	RequestHook onResolveEntity;		//!< maps the request URI into local physical path.
	RequestHook onPostProcess;			//!< gets invoked right before serializing headers
	RequestHook onRequestDone;			//!< this hook is invoked once the request has been <b>fully</b> served to the client.
	ConnectionHook onConnectionClose;	//!< is called before a connection gets closed / or has been closed by remote point.
	ConnectionStatusHook onConnectionStatusChanged; //!< is invoked whenever a the connection status changes.
	// }}}

	WorkerHook onWorkerSpawn;
	WorkerHook onWorkerUnspawn;

	unsigned generation() const { return generation_; }

	void addComponent(const std::string& value);

	/**
	 * writes a log entry into the server's error log.
	 */
	template<typename... Args>
	void log(Severity s, const char* fmt, Args... args)
	{
		log(LogMessage(s, fmt, args...));
	}

	void log(LogMessage&& msg);

	Severity logLevel() const;
	void logLevel(Severity value);

	ServerSocket* setupListener(const std::string& bindAddress, int port, int backlog = 0 /*default*/);
	ServerSocket* setupUnixListener(const std::string& path, int backlog = 0 /*default*/);
	ServerSocket* setupListener(const SocketSpec& spec);
	void destroyListener(ServerSocket* listener);

	std::string pluginDirectory() const;
	void setPluginDirectory(const std::string& value);

	HttpPlugin* loadPlugin(const std::string& name, std::error_code& ec);
	template<typename T> T* loadPlugin(const std::string& name, std::error_code& ec);
	void unloadPlugin(const std::string& name);
	std::vector<std::string> pluginsLoaded() const;

	HttpPlugin* registerPlugin(HttpPlugin* plugin);
	HttpPlugin* unregisterPlugin(HttpPlugin* plugin);

	struct ::ev_loop* loop() const;

	HttpCore& core() const;

	const std::list<ServerSocket*>& listeners() const;

	void dumpIR() const; // for debugging purpose

	friend class HttpConnection;
	friend class HttpPlugin;
	friend class HttpWorker;
	friend class HttpCore;

public: // FlowBackend overrides
	virtual void import(const std::string& name, const std::string& path);

	// setup
	bool registerSetupFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);
	bool registerSetupProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);

	// shared
	bool registerSharedFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);
	bool registerSharedProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);

	// main
	bool registerHandler(const std::string& name, CallbackFunction callback, void* userdata = nullptr);
	bool registerFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);
	bool registerProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);

private:
#if defined(WITH_SSL)
	static void gnutls_log(int level, const char* msg);
#endif

	bool validateConfig();

	void onNewConnection(Socket*, ServerSocket*);

	unsigned generation_;
	std::vector<std::string> components_;

	Unit* unit_;
	FlowRunner* runner_;
	std::vector<std::string> setupApi_;
	std::vector<std::string> mainApi_;

	std::list<ServerSocket*> listeners_;
	struct ::ev_loop* loop_;
	ev_tstamp startupTime_;
	LoggerPtr logger_;
	Severity logLevel_;
	bool colored_log_;
	std::string pluginDirectory_;
	std::vector<HttpPlugin*> plugins_;
	std::unordered_map<HttpPlugin*, Library> pluginLibraries_;
	HttpCore* core_;
	std::atomic<unsigned int> workerIdPool_;
	std::vector<HttpWorker*> workers_;
	size_t lastWorker_;
	FileInfoService::Config fileinfoConfig_;

public:
	ValueProperty<std::size_t> maxConnections;
	ValueProperty<TimeSpan> maxKeepAlive;
	ValueProperty<std::size_t> maxKeepAliveRequests;
	ValueProperty<TimeSpan> maxReadIdle;
	ValueProperty<TimeSpan> maxWriteIdle;
	ValueProperty<bool> tcpCork;
	ValueProperty<bool> tcpNoDelay;
	ValueProperty<TimeSpan> lingering;
	ValueProperty<std::string> tag;
	ValueProperty<bool> advertise;

	ValueProperty<std::size_t> maxRequestUriSize;
	ValueProperty<std::size_t> maxRequestHeaderSize;
	ValueProperty<std::size_t> maxRequestHeaderCount;
	ValueProperty<std::size_t> maxRequestBodySize;
};

// {{{ inlines
inline void HttpServer::setLogger(std::shared_ptr<Logger> logger)
{
	logger_ = logger;
	logger_->setLevel(logLevel_);
}

inline Logger *HttpServer::logger() const
{
	return logger_.get();
}

inline struct ::ev_loop* HttpServer::loop() const
{
	return loop_;
}

inline HttpCore& HttpServer::core() const
{
	return *core_;
}

inline const std::list<ServerSocket*>& HttpServer::listeners() const
{
	return listeners_;
}

template<typename T>
inline T *HttpServer::loadPlugin(const std::string& name, std::error_code& ec)
{
	return dynamic_cast<T*>(loadPlugin(name, ec));
}

inline Severity HttpServer::logLevel() const
{
	return logLevel_;
}

inline void HttpServer::logLevel(Severity value)
{
	logLevel_ = value;
	logger()->setLevel(value);
}
// }}}

//@}

} // namespace x0

#endif
// vim:syntax=cpp
