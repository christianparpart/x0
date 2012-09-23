/* <x0/HttpServer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpCore.h>
#include <x0/ServerSocket.h>
#include <x0/SocketSpec.h>
#include <x0/Error.h>
#include <x0/Logger.h>
#include <x0/Library.h>
#include <x0/AnsiColor.h>
#include <x0/strutils.h>
#include <x0/sysconfig.h>

#include <x0/flow/FlowRunner.h>

#include <sd-daemon.h>

#include <iostream>
#include <fstream>
#include <cstdarg>
#include <cstdlib>

#if defined(HAVE_SYS_UTSNAME_H)
#	include <sys/utsname.h>
#endif

#if defined(HAVE_ZLIB_H)
#	include <zlib.h>
#endif

#if defined(HAVE_BZLIB_H)
#	include <bzlib.h>
#endif

#include <sys/resource.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <stdio.h>

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif


namespace x0 {

void wrap_log_error(HttpServer *srv, const char *cat, const std::string& msg)
{
	srv->log(Severity::error, "%s: %s", cat, msg.c_str());
}

X0_EXPORT std::string global_now()
{
	float val = ev_now(ev_default_loop(0));
	time_t ts = (time_t)val;
	struct tm tm;

	if (localtime_r(&ts, &tm)) {
		char buf[256];

		if (strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", &tm) != 0) {
			return buf;
		}
	}

	return "unknown";
}

/** initializes the HTTP server object.
 * \param io_service an Asio io_service to use or nullptr to create our own one.
 * \see HttpServer::run()
 */
HttpServer::HttpServer(struct ::ev_loop *loop, unsigned generation) :
#ifndef NDEBUG
	Logging("HttpServer"),
#endif
	onConnectionOpen(),
	onPreProcess(),
	onResolveDocumentRoot(),
	onResolveEntity(),
	onPostProcess(),
	onRequestDone(),
	onConnectionClose(),
	onWorkerSpawn(),
	onWorkerUnspawn(),

	generation_(generation),
	components_(),

	unit_(nullptr),
	runner_(nullptr),
	setupApi_(),
	mainApi_(),
	onHandleRequest_(),

	listeners_(),
	loop_(loop ? loop : ev_default_loop(0)),
	startupTime_(ev_now(loop_)),
	logger_(),
	logLevel_(Severity::warn),
	colored_log_(false),
	pluginDirectory_(PLUGINDIR),
	plugins_(),
	pluginLibraries_(),
	core_(0),
	workers_(),
	lastWorker_(0),
	maxConnections(512),
	maxKeepAlive(TimeSpan::fromSeconds(60)),
	maxKeepAliveRequests(100),
	maxReadIdle(TimeSpan::fromSeconds(60)),
	maxWriteIdle(TimeSpan::fromSeconds(360)),
	tcpCork(false),
	tcpNoDelay(false),
	tag("x0/" VERSION),
	advertise(true),
	maxRequestUriSize(4 * 1024),
	maxRequestHeaderSize(8 * 1024),
	maxRequestHeaderCount(100),
	maxRequestBodySize(2 * 1024 * 1024)
{
	runner_ = new FlowRunner(this);
	runner_->setErrorHandler(std::bind(&wrap_log_error, this, "codegen", std::placeholders::_1));

	HttpRequest::initialize();

	auto nowfn = std::bind(&global_now);
	logger_.reset(new FileLogger<decltype(nowfn)>("/dev/stderr", nowfn));

	struct rlimit rlim;
	if (::getrlimit(RLIMIT_NOFILE, &rlim) == 0)
		maxConnections = rlim.rlim_cur / 2;

	registerPlugin(core_ = new HttpCore(*this));

	// spawn main-thread worker
	spawnWorker();
}

HttpServer::~HttpServer()
{
	TRACE("destroying");
	stop();

	for (auto i: listeners_)
		delete i;

	unregisterPlugin(core_);
	delete core_;
	core_ = nullptr;

	while (!plugins_.empty())
		unloadPlugin(plugins_[plugins_.size() - 1]->name());

	while (!workers_.empty())
		destroyWorker(workers_[workers_.size() - 1]);

	delete runner_;
	runner_ = nullptr;

	delete unit_;
	unit_ = nullptr;
}

bool HttpServer::validateConfig()
{
	TRACE("validateConfig()");

	Function* setupFn = runner_->findHandler("setup");
	if (!setupFn) {
		log(Severity::error, "no setup-handler defined in config file.");
		return false;
	}

	Function* mainFn = runner_->findHandler("main");
	if (!mainFn) {
		log(Severity::error, "no main-handler defined in config file.");
		return false;
	}

	unsigned errors = 0;

	TRACE("validateConfig: setup:");
	for (auto& i: FlowCallIterator(setupFn)) {
		if (i->callee()->body())
			// skip script user-defined handlers
			continue;

		TRACE(" - %s %ld", i->callee()->name().c_str(), i->args()->size());

		if (std::find( setupApi_.begin(), setupApi_.end(), i->callee()->name()) == setupApi_.end()) {
			log(Severity::error, "Symbol '%s' found within setup-handler (or its callees) but may be only invoked from within the main-handler.", i->callee()->name().c_str());
			log(Severity::error, "%s", i->sourceLocation().dump().c_str());
			++errors;
		}
	}

	TRACE("validateConfig: main:");
	for (auto& i: FlowCallIterator(mainFn)) {
		if (i->callee()->body())
			// skip script user-defined handlers
			continue;

		TRACE(" - %s %ld", i->callee()->name().c_str(), i->args()->size());

		if (std::find(mainApi_.begin(), mainApi_.end(), i->callee()->name()) == mainApi_.end()) {
			log(Severity::error, "Symbol '%s' found within main-handler (or its callees) but may be only invoked from within the setup-handler.", i->callee()->name().c_str());
			log(Severity::error, "%s", i->sourceLocation().dump().c_str());
			++errors;
		}
	}

	TRACE("validateConfig finished");
	return errors == 0;
}

void HttpServer::onNewConnection(Socket* cs, ServerSocket* ss)
{
	selectWorker()->enqueue(std::make_pair(cs, ss));
}

bool HttpServer::setup(std::istream *settings, const std::string& filename, int optimizationLevel)
{
	TRACE("setup(%s)", filename.c_str());

	runner_->setErrorHandler(std::bind(&wrap_log_error, this, "parser", std::placeholders::_1));
	runner_->setOptimizationLevel(optimizationLevel);

	if (!runner_->open(filename)) {
		sd_notifyf(0, "ERRNO=%d", errno);
		goto err;
	}

	if (!validateConfig())
		goto err;

	// run setup
	TRACE("run 'setup'");
	if (runner_->invoke(runner_->findHandler("setup")))
		goto err;

	// grap the request handler
	TRACE("get pointer to 'main'");
	onHandleRequest_ = runner_->getPointerTo(runner_->findHandler("main"));

	if (!onHandleRequest_)
		goto err;

	// {{{ setup server-tag
	{
#if defined(HAVE_SYS_UTSNAME_H)
		{
			utsname utsname;
			if (uname(&utsname) == 0)
			{
				components_.insert(components_.begin(), 
					std::string(utsname.sysname) + "/" + utsname.release
				);

				components_.insert(components_.begin(), utsname.machine);
			}
		}
#endif

#if defined(HAVE_BZLIB_H)
		{
			std::string zver("bzip2/");
			zver += BZ2_bzlibVersion();
			zver = zver.substr(0, zver.find(","));
			components_.insert(components_.begin(), zver);
		}
#endif

#if defined(HAVE_ZLIB_H)
		{
			std::string zver("zlib/");
			zver += zlib_version;
			components_.insert(components_.begin(), zver);
		}
#endif

		Buffer tagbuf;
		tagbuf.push_back("x0/" VERSION);

		if (!components_.empty())
		{
			tagbuf.push_back(" (");

			for (int i = 0, e = components_.size(); i != e; ++i)
			{
				if (i)
					tagbuf.push_back(", ");

				tagbuf.push_back(components_[i]);
			}

			tagbuf.push_back(")");
		}
		tag = tagbuf.str();
	}
	// }}}

	// {{{ run post-config hooks
	TRACE("setup: post_config");
	for (auto i: plugins_)
		if (!i->post_config())
			goto err;
	// }}}

	// {{{ run post-check hooks
	TRACE("setup: post_check");
	for (auto i: plugins_)
		if (!i->post_check())
			goto err;
	// }}}

	// {{{ check for available TCP listeners
	if (listeners_.empty()) {
		log(Severity::error, "No HTTP listeners defined");
		goto err;
	}
	for (auto i: listeners_)
		if (!i->isOpen())
			goto err;
	// }}}

	// {{{ x0d: check for superfluous passed file descriptors (and close them)
	for (auto fd: ServerSocket::getInheritedSocketList()) {
		bool found = false;
		for (auto li: listeners_) {
			if (fd == li->handle()) {
				found = true;
				break;
			}
		}
		if (!found) {
			log(Severity::debug, "Closing inherited superfluous listening socket %d.", fd);
			::close(fd);
		}
	}
	// }}}

	// {{{ systemd: check for superfluous passed file descriptors
	if (int count = sd_listen_fds(0)) {
		int maxfd = SD_LISTEN_FDS_START + count;
		count = 0;
		for (int fd = SD_LISTEN_FDS_START; fd < maxfd; ++fd) {
			bool found = false;
			for (auto li: listeners_) {
				if (fd == li->handle()) {
					found = true;
					break;
				}
			}
			if (!found) {
				++count;
			}
		}
		if (count) {
			fprintf(stderr, "superfluous systemd file descriptors: %d\n", count);
			return false;
		}
	}
	// }}}

	// XXX post worker wakeup
	// we do an explicit wakeup of all workers here since there might be already
	// some (configure-time related) events pending, i.e. director's (fcgi) health checker
	// FIXME this is more a workaround than a fix.
	for (auto worker: workers_)
		worker->wakeup();

	TRACE("setup: done.");
	return true;

err:
	return false;
}

bool HttpServer::setup(const std::string& filename, int optimizationLevel)
{
	std::ifstream s(filename);
	return setup(&s, filename, optimizationLevel);
}

// {{{ worker mgnt
HttpWorker *HttpServer::spawnWorker()
{
	struct ev_loop *loop = !workers_.empty()
		? ev_loop_new(0)
		: loop_;

	HttpWorker *worker = new HttpWorker(*this, loop);

	if (!workers_.empty())
		pthread_create(&worker->thread_, nullptr, &HttpServer::runWorker, worker);

	workers_.push_back(worker);

	return worker;
}

/**
 * Selects (by round-robin) the next worker.
 *
 * \return a worker
 * \note This method is not thread-safe, and thus, should not be invoked within a request handler.
 */
HttpWorker* HttpServer::nextWorker()
{
	if (++lastWorker_ == workers_.size())
		lastWorker_ = 0;

	return workers_[lastWorker_];
}

HttpWorker *HttpServer::selectWorker()
{
#if defined(X0_WORKER_RR)
	// select by RR (round-robin)
	// this is thread-safe since only one thread is to select a new worker
	// (the main thread)
	if (++lastWorker_ == workers_.size())
		lastWorker_ = 0;

	return workers_[lastWorker_];
#else
	// select by lowest connection load
	HttpWorker *best = workers_[0];
	int value = best->connectionLoad();

	for (size_t i = 1, e = workers_.size(); i != e; ++i) {
		HttpWorker *w = workers_[i];
		int l = w->connectionLoad();
		if (l < value) {
			value = l;
			best = w;
		}
	}

	return best;
#endif
}

void HttpServer::destroyWorker(HttpWorker *worker)
{
	auto i = std::find(workers_.begin(), workers_.end(), worker);
	assert(i != workers_.end());

	worker->stop();

	if (worker != workers_.front())
		pthread_join(worker->thread_, nullptr);

	workers_.erase(i);
	delete worker;
}

void *HttpServer::runWorker(void *p)
{
	HttpWorker *w = (HttpWorker *)p;
	w->run();
	return nullptr;
}
// }}}

/** calls run on the internally referenced io_service.
 * \note use this if you do not have your own main loop.
 * \note automatically starts the server if it wasn't started via \p start() yet.
 * \see start(), stop()
 */
int HttpServer::run()
{
	workers_.front()->run();

	return 0;
}

/**
 * retrieves the listener object that is responsible for the given port number, or null otherwise.
 */
ServerSocket* HttpServer::listenerByPort(int port) const
{
	for (auto listener: listeners_)
		if (listener->port() == port)
			return listener;

	return nullptr;
}

/** unregisters all listeners from the underlying io_service and calls stop on it.
 * \see start(), run()
 */
void HttpServer::stop()
{
	for (auto listener: listeners_)
		listener->stop();

	for (auto worker: workers_)
		worker->stop();
}

void HttpServer::kill()
{
	stop();

	for (auto worker: workers_)
		worker->kill();
}

void HttpServer::log(Severity s, const char *msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buf[512];
	int buflen = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	if (colored_log_)
	{
		static AnsiColor::Type colors[] = {
			AnsiColor::Red | AnsiColor::Bold, // error
			AnsiColor::Yellow | AnsiColor::Bold, // warn
			AnsiColor::Green, // info
			static_cast<AnsiColor::Type>(0),
			AnsiColor::Cyan, // debug
		};

		Buffer sb;
		sb.push_back(AnsiColor::make(colors[s + 3]));
		sb.push_back(buf, buflen);
		sb.push_back(AnsiColor::make(AnsiColor::Clear));

		if (logger_)
			logger_->write(s, sb.str());
		else
			std::fprintf(stderr, "%s\n", sb.c_str());
	}
	else
	{
		if (logger_)
			logger_->write(s, buf);
		else
			std::fprintf(stderr, "%s\n", buf);
	}
}

/**
 * sets up a TCP/IP ServerSocket on given bind_address and port.
 *
 * If there is already a ServerSocket on this bind_address:port pair
 * then no error will be raised.
 */
ServerSocket* HttpServer::setupListener(const std::string& bind_address, int port, int backlog)
{
	return setupListener(SocketSpec::fromInet(IPAddress(bind_address), port, backlog));
}

ServerSocket *HttpServer::setupUnixListener(const std::string& path, int backlog)
{
	return setupListener(SocketSpec::fromLocal(path, backlog));
}

namespace {
	static inline Buffer readFile(const char* path)
	{
		FILE* fp = fopen(path, "r");
		if (!fp)
			return Buffer();

		Buffer result;
		char buf[4096];

		while (!feof(fp)) {
			size_t n = fread(buf, 1, sizeof(buf), fp);
			result.push_back(buf, n);
		}

		fclose(fp);

		return result;
	}

	template<typename T>
	static inline T readFile(const char* path, const T& defaultValue)
	{
		Buffer result(readFile(path));
		return !result.empty() ? result.ref().as<T>() : defaultValue;
	}
}

ServerSocket* HttpServer::setupListener(const SocketSpec& _spec)
{
	// validate backlog against system's hard limit
	SocketSpec spec(_spec);
	int somaxconn = readFile<int>("/proc/sys/net/core/somaxconn", 0);
	if (spec.backlog() > 0) {
		if (somaxconn && spec.backlog() > somaxconn) {
			log(Severity::error,
				"Listener %s configured with a backlog higher than the system permits (%ld > %ld). "
				"See /proc/sys/net/core/somaxconn for your system limits.",
				spec.str().c_str(), spec.backlog(), somaxconn);

			return nullptr;
		}
	}

	// create a new listener
	ServerSocket* lp = new ServerSocket(loop_);
	lp->set<HttpServer, &HttpServer::onNewConnection>(this);

	listeners_.push_back(lp);

	if (spec.backlog() <= 0)
		lp->setBacklog(somaxconn);

	if (lp->open(spec, O_NONBLOCK | O_CLOEXEC)) {
		log(Severity::info, "Listening on %s", spec.str().c_str());
		return lp;
	} else {
		log(Severity::error, "Could not create listener %s: %s", spec.str().c_str(), lp->errorText().c_str());
		return nullptr;
	}
}

void HttpServer::destroyListener(ServerSocket* listener)
{
	for (auto i = listeners_.begin(), e = listeners_.end(); i != e; ++i) {
		if (*i == listener) {
			listeners_.erase(i);
			delete listener;
			break;
		}
	}
}

typedef HttpPlugin *(*plugin_create_t)(HttpServer&, const std::string&);

std::string HttpServer::pluginDirectory() const
{
	return pluginDirectory_;
}

void HttpServer::setPluginDirectory(const std::string& value)
{
	pluginDirectory_ = value;
}

// {{{ Flow helper
void HttpServer::import(const std::string& name, const std::string& path)
{
	std::string filename = path;
	if (!filename.empty() && filename[filename.size() - 1] != '/')
		filename += "/";
	filename += name;

	std::error_code ec;
	loadPlugin(filename, ec);

	if (ec)
		log(Severity::error, "Error loading plugin: %s: %s", filename.c_str(), ec.message().c_str());
}

// setup
bool HttpServer::registerSetupFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata)
{
	setupApi_.push_back(name);
	return FlowBackend::registerFunction(name, returnType, callback, userdata);
}

bool HttpServer::registerSetupProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata)
{
	setupApi_.push_back(name);
	return FlowBackend::registerProperty(name, returnType, callback, userdata);
}

// shared
bool HttpServer::registerSharedFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata)
{
	setupApi_.push_back(name);
	mainApi_.push_back(name);
	return FlowBackend::registerFunction(name, returnType, callback, userdata);
}

bool HttpServer::registerSharedProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata)
{
	setupApi_.push_back(name);
	mainApi_.push_back(name);
	return FlowBackend::registerProperty(name, returnType, callback, userdata);
}

// main
bool HttpServer::registerHandler(const std::string& name, CallbackFunction callback, void* userdata)
{
	mainApi_.push_back(name);
	return FlowBackend::registerHandler(name, callback, userdata);
}

bool HttpServer::registerFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata)
{
	mainApi_.push_back(name);
	return FlowBackend::registerFunction(name, returnType, callback, userdata);
}

bool HttpServer::registerProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata)
{
	mainApi_.push_back(name);
	return FlowBackend::registerProperty(name, returnType, callback, userdata);
}
// }}}

HttpServer* HttpServer::fromText(const std::string& configText, std::future<int>** async)
{
	HttpServer* server = new HttpServer(ev_default_loop(0), 1);
	std::istringstream config(configText);
	server->setup(&config, "createFromText", 2);

	if (!async)
		return server;

	*async = new std::future<int>(std::move(std::async(std::launch::async, [&]() {
		std::shared_ptr<HttpServer> sp(server);
		return server->run();
	})));

	return server;
}

/**
 * loads a plugin into the server.
 *
 * \see plugin, unload_plugin(), loaded_plugins()
 */
HttpPlugin *HttpServer::loadPlugin(const std::string& name, std::error_code& ec)
{
	if (!pluginDirectory_.empty() && pluginDirectory_[pluginDirectory_.size() - 1] != '/')
		pluginDirectory_ += "/";

	std::string filename;
	if (name.find('/') != std::string::npos)
		filename = name + ".so";
	else
		filename = pluginDirectory_ + name + ".so";

	std::string plugin_create_name("x0plugin_init");

#if !defined(NDEBUG)
	log(Severity::debug, "Loading plugin %s", filename.c_str());
#endif

	Library lib;
	ec = lib.open(filename);
	if (!ec)
	{
		plugin_create_t plugin_create = reinterpret_cast<plugin_create_t>(lib.resolve(plugin_create_name, ec));

		if (!ec)
		{
			HttpPlugin *plugin = plugin_create(*this, name);
			pluginLibraries_[plugin] = std::move(lib);

			return registerPlugin(plugin);
		}
	}

	return nullptr;
}

/** safely unloads a plugin. */
void HttpServer::unloadPlugin(const std::string& name)
{
#if !defined(NDEBUG)
	log(Severity::debug, "Unloading plugin: %s", name.c_str());
#endif

	for (auto plugin: plugins_)
	{
		if (plugin->name() == name)
		{
			unregisterPlugin(plugin);

			auto m = pluginLibraries_.find(plugin);
			if (m != pluginLibraries_.end())
			{
				delete plugin;
				m->second.close();
				pluginLibraries_.erase(m);
			}
			return;
		}
	}
}

/** retrieves a list of currently loaded plugins */
std::vector<std::string> HttpServer::pluginsLoaded() const
{
	std::vector<std::string> result;

	for (auto plugin: plugins_)
		result.push_back(plugin->name());

	return result;
}

HttpPlugin *HttpServer::registerPlugin(HttpPlugin *plugin)
{
	plugins_.push_back(plugin);

	return plugin;
}

HttpPlugin *HttpServer::unregisterPlugin(HttpPlugin *plugin)
{
	auto i = std::find(plugins_.begin(), plugins_.end(), plugin);
	if (i != plugins_.end()) {
		unregisterNative(plugin->name());
		plugins_.erase(i);
	}

	return plugin;
}

void HttpServer::addComponent(const std::string& value)
{
	components_.push_back(value);
}

void HttpServer::dumpIR() const
{
	runner_->dump();
}

} // namespace x0
