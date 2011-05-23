/* <x0/HttpServer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpServer.h>
#include <x0/http/HttpListener.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpCore.h>
#include <x0/Error.h>
#include <x0/Logger.h>
#include <x0/Library.h>
#include <x0/AnsiColor.h>
#include <x0/strutils.h>
#include <x0/sysconfig.h>

#include <x0/flow/FlowRunner.h>

#include <sd-daemon.h>

#include <iostream>
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

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->debug(0, msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif


namespace x0 {

void wrap_log_error(HttpServer *srv, const char *cat, const std::string& msg)
{
	//fprintf(stderr, "%s: %s\n", cat, msg.c_str());
	//fflush(stderr);
	srv->log(Severity::error, "%s: %s", cat, msg.c_str());
}

std::string global_now()
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
HttpServer::HttpServer(struct ::ev_loop *loop) :
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
	components_(),

	unit_(nullptr),
	runner_(nullptr),
	onHandleRequest_(),

	listeners_(),
	loop_(loop ? loop : ev_default_loop(0)),
	startupTime_(ev_now(loop_)),
	active_(false),
	logger_(),
	logLevel_(Severity::warn),
	colored_log_(false),
	pluginDirectory_(PLUGINDIR),
	plugins_(),
	pluginLibraries_(),
	core_(0),
	workers_(),
#if defined(X0_WORKER_RR)
	lastWorker_(0),
#endif
	maxConnections(512),
	maxKeepAlive(TimeSpan::fromSeconds(60)),
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

	registerPlugin(core_ = new HttpCore(*this));

	sd_notify(0, "STATUS=Initialized");
}

HttpServer::~HttpServer()
{
	TRACE("destroying");
	stop();

	for (auto i: listeners_)
		delete i;

	while (!workers_.empty())
		destroyWorker(workers_[workers_.size() - 1]);

	unregisterPlugin(core_);
	delete core_;
	core_ = nullptr;

	while (!plugins_.empty())
		unloadPlugin(plugins_[plugins_.size() - 1]->name());

	delete runner_;
	runner_ = nullptr;

	delete unit_;
	unit_ = nullptr;
}

bool HttpServer::setup(std::istream *settings, const std::string& filename)
{
	sd_notify(0, "STATUS=Setting up");

	runner_->setErrorHandler(std::bind(&wrap_log_error, this, "parser", std::placeholders::_1));
	if (!runner_->open(filename)) {
		sd_notifyf(0, "ERRNO=%d", errno);
		goto err;
	}

	// run setup
	{
		Function* setupFn = runner_->findHandler("setup");
		if (!setupFn) {
			log(Severity::error, "no setup handler defined in config file.\n");
			goto err;
		}

		if (runner_->invoke(setupFn))
			goto err;
	}

	// grap the request handler
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
	for (auto i: plugins_)
		if (!i->post_config())
			goto err;
	// }}}

	// {{{ run post-check hooks
	for (auto i: plugins_)
		if (!i->post_check())
			goto err;
	// }}}

	// {{{ check for available TCP listeners
	if (listeners_.empty())
	{
		log(Severity::error, "No HTTP listeners defined");
		goto err;
	}
	// }}}

	sd_notify(0, "STATUS=Setup done");
	return true;

err:
	sd_notify(0, "STATUS=Setup failed");
	return false;
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

HttpWorker *HttpServer::selectWorker()
{
#if defined(X0_WORKER_RR)
	// select by RR (round-robin)
	// this is thread-safe since only one thread is to select a new worker
	// (the main thread: HttpListener)
	if (++lastWorker_ == workers_.size())
		lastWorker_ = 0;

	return workers_[lastWorker_];
#else
	// select by lowest connection load
	HttpWorker *best = workers_[0];
	int value = 1;

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

	worker->evExit_.send();

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

/** starts the HTTP server by starting all listeners.
 *
 * @see setup(), setupListener()
 * @note also spawns one worker if no workers has been spawned yet.
 */
bool HttpServer::start()
{
	if (!active_)
	{
		if (workers_.empty())
			spawnWorker();

		active_ = true;

		for (auto i: listeners_)
			if (i->errorCount())
				return false;

		// systemd: check for superfluous passed file descriptors
		int count = sd_listen_fds(0);
		if (count > 0) {
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
	}

	return true;
}

/** tests whether this server has been started or not.
 * \see start(), run()
 */
bool HttpServer::active() const
{
	return active_;
}

/** calls run on the internally referenced io_service.
 * \note use this if you do not have your own main loop.
 * \note automatically starts the server if it wasn't started via \p start() yet.
 */
int HttpServer::run()
{
	if (!active_)
	{
		if (!start())
			return -1;
	}

	sd_notify(0, "READY=1\n"
				 "STATUS=Accepting requests ...");

	while (active_)
	{
		workers_.front()->run();
	}

	return 0;
}

/**
 * retrieves the listener object that is responsible for the given port number, or null otherwise.
 */
HttpListener *HttpServer::listenerByPort(int port) const
{
	for (auto listener: listeners_)
		if (listener->socket().port() == port)
			return listener;

	return nullptr;
}

void HttpServer::pause()
{
	active_ = false;
	sd_notify(0, "STATUS=Paused");
}

void HttpServer::resume()
{
	active_ = true;
	sd_notify(0, "STATUS=Accepting requests ...");
}

void HttpServer::reload()
{
	//! \todo implementation
}

/** unregisters all listeners from the underlying io_service and calls stop on it.
 * \see start(), active(), run()
 */
void HttpServer::stop()
{
	if (active_)
	{
		sd_notify(0, "STATUS=Stopping ...");
		active_ = false;

		for (auto listener: listeners_)
			listener->stop();

		for (auto worker: workers_)
			worker->stop();
	}
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
 * sets up a TCP/IP HttpListener on given bind_address and port.
 *
 * If there is already a HttpListener on this bind_address:port pair
 * then no error will be raised.
 */
HttpListener *HttpServer::setupListener(const std::string& bind_address, int port, int backlog)
{
	// check if we already have an HTTP listener listening on given port
//	if (HttpListener *lp = listenerByPort(port))
//		return lp;

	// create a new listener
	HttpListener *lp = new HttpListener(*this);

	if (backlog)
		lp->setBacklog(backlog);

	listeners_.push_back(lp);

	if (lp->open(bind_address, port))
		return lp;

	return nullptr;
}

HttpListener *HttpServer::setupUnixListener(const std::string& path, int backlog)
{
	// create a new listener
	HttpListener *lp = new HttpListener(*this);

	if (backlog)
		lp->setBacklog(backlog);

	listeners_.push_back(lp);

	if (lp->open(path))
		return lp;

	return nullptr;
}

HttpListener* HttpServer::setupListener(const SocketSpec& spec)
{
	// create a new listener
	HttpListener *lp = new HttpListener(*this);

	listeners_.push_back(lp);

	if (lp->open(spec))
		return lp;

	return nullptr;
}

void HttpServer::destroyListener(HttpListener *listener)
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
