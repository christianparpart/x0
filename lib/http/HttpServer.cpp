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
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpCore.h>
#include <x0/Error.h>
#include <x0/Logger.h>
#include <x0/Library.h>
#include <x0/AnsiColor.h>
#include <x0/strutils.h>
#include <x0/sysconfig.h>

#include <flow/Flow.h>
#include <flow/Value.h>
#include <flow/Parser.h>
#include <flow/Runner.h>

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

#define TRACE(level, msg...) debug(level, msg)

namespace x0 {

void wrap_log_error(HttpServer *srv, const char *cat, const std::string& msg)
{
	//fprintf(stderr, "%s: %s\n", cat, msg.c_str());
	//fflush(stderr);
	srv->log(Severity::error, "%s: %s", cat, msg.c_str());
}

/** initializes the HTTP server object.
 * \param io_service an Asio io_service to use or NULL to create our own one.
 * \see HttpServer::run()
 */
HttpServer::HttpServer(struct ::ev_loop *loop) :
	onConnectionOpen(),
	onPreProcess(),
	onResolveDocumentRoot(),
	onResolveEntity(),
	onPostProcess(),
	onRequestDone(),
	onConnectionClose(),
	components_(),

	unit_(NULL),
	runner_(NULL),
	onHandleRequest_(),

	listeners_(),
	loop_(loop ? loop : ev_default_loop(0)),
	active_(false),
	logger_(),
	logLevel_(Severity::warn),
	colored_log_(false),
	pluginDirectory_(PLUGINDIR),
	plugins_(),
	pluginLibraries_(),
	core_(0),
	workers_(),
	max_connections(512),
	max_keep_alive_idle(/*5*/ 60),
	max_read_idle(60),
	max_write_idle(360),
	tcp_cork(false),
	tcp_nodelay(false),
	tag("x0/" VERSION),
	advertise(true)
{
	runner_ = new Flow::Runner(this);
	runner_->setErrorHandler(std::bind(&wrap_log_error, this, "codegen", std::placeholders::_1));

	HttpResponse::initialize();

	spawnWorker(); // main worker

	auto nowfn = std::bind(&DateTime::htlog_str, &workers_[0]->now_);
	logger_.reset(new FileLogger<decltype(nowfn)>("/dev/stderr", nowfn));

	registerPlugin(core_ = new HttpCore(*this));
}

HttpServer::~HttpServer()
{
	stop();

	for (std::list<HttpListener *>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
		delete *k;

	while (!workers_.empty())
		destroyWorker(workers_[workers_.size() - 1]);

	unregisterPlugin(core_);
	delete core_;
	core_ = 0;

	while (!plugins_.empty())
		unloadPlugin(plugins_[plugins_.size() - 1]->name());

	delete runner_;
	runner_ = NULL;

	delete unit_;
	unit_ = NULL;
}

bool HttpServer::setup(std::istream *settings)
{
	Flow::Parser parser;
	parser.setErrorHandler(std::bind(&wrap_log_error, this, "parser", std::placeholders::_1));
	if (!parser.initialize(settings)) {
		perror("open");
		return false;
	}

	unit_ = parser.parse();
	if (!unit_)
		return false;

	Flow::Function *setupFn = unit_->lookup<Flow::Function>("setup");
	if (!setupFn) {
		log(Severity::error, "no setup handler defined in config file.\n");
		return false;
	}

	// compile module
	runner_->compile(unit_);

	// run setup
	if (runner_->run(setupFn))
		return false;

	// grap the request handler
	onHandleRequest_ = runner_->compile(unit_->lookup<Flow::Function>("main"));
	if (!onHandleRequest_)
		return false;

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
	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		if (!(*i)->post_config())
			return false;
	// }}}

	// {{{ run post-check hooks
	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		if (!(*i)->post_check())
			return false;
	// }}}

	// {{{ check for available TCP listeners
	if (listeners_.empty())
	{
		log(Severity::error, "No HTTP listeners defined");
		return false;
	}

	for (auto i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
		if (!(*i)->prepare())
			return false;
	// }}}

	return true;
}

// {{{ worker mgnt
HttpWorker *HttpServer::spawnWorker()
{
	struct ev_loop *loop = !workers_.empty()
		? ev_loop_new(0)
		: loop_;

	HttpWorker *worker = new HttpWorker(*this, loop);

	if (!workers_.empty())
	{
		pthread_create(&worker->thread_, NULL, &HttpServer::runWorker, worker);
	}

	workers_.push_back(worker);

	return worker;
}

HttpWorker *HttpServer::selectWorker()
{
	HttpWorker *best = workers_[0];
	unsigned value = 1;

	for (size_t i = 1, e = workers_.size(); i != e; ++i)
	{
		HttpWorker *w = workers_[i];
		if (w->load() < value) {
			value = best->load();
			best = w;
		}
	}

	return best;
}

void HttpServer::destroyWorker(HttpWorker *worker)
{
	std::vector<HttpWorker *>::iterator i = workers_.begin();
	while (i != workers_.end())
	{
		if (*i == worker)
		{
			worker->evExit_.send();

			if (worker != workers_.front())
				pthread_join(worker->thread_, NULL);

			delete worker;
			workers_.erase(i);
			return;
		}
		++i;
	}
}

void *HttpServer::runWorker(void *p)
{
	HttpWorker *w = (HttpWorker *)p;
	w->run();
	return NULL;
}
// }}}

bool HttpServer::start()
{
	if (!active_)
	{
		active_ = true;

		for (std::list<HttpListener *>::iterator i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
			if (!(*i)->start())
				return false;
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
void HttpServer::run()
{
	if (!active_)
		start();

	while (active_)
	{
		workers_.front()->run();
	}
}

/**
 * retrieves the listener object that is responsible for the given port number, or null otherwise.
 */
HttpListener *HttpServer::listenerByPort(int port) const
{
	for (auto k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		HttpListener *http_server = *k;

		if (http_server->port() == port)
		{
			return http_server;
		}
	}

	return 0;
}

void HttpServer::pause()
{
	active_ = false;
}

void HttpServer::resume()
{
	active_ = true;
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
		active_ = false;

		for (std::list<HttpListener *>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
		{
			(*k)->stop();
		}

		for (auto i = workers_.begin(), e = workers_.end(); i != e; ++i)
			(*i)->evExit_.send();

		//ev_unloop(loop_, ev::ALL);
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
HttpListener *HttpServer::setupListener(int port, const std::string& bind_address)
{
	// check if we already have an HTTP listener listening on given port
	if (HttpListener *lp = listenerByPort(port))
		return lp;

	// create a new listener
	HttpListener *lp = new HttpListener(*this);

	lp->address(bind_address);
	lp->port(port);

	// TODO: configurable listener backlog
#if 0
	int value = 0;
	if (!settings_.load("Resources.MaxConnections", value))
		lp->backlog(value);
#endif

	listeners_.push_back(lp);

	//log(Severity::debug, "Listening on %s:%d", bind_address.c_str(), port);

	return lp;
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

	return NULL;
}

/** safely unloads a plugin. */
void HttpServer::unloadPlugin(const std::string& name)
{
#if !defined(NDEBUG)
	//log(Severity::debug, "Unloading plugin: %s", name.c_str());
#endif

	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
	{
		HttpPlugin *plugin = *i;

		if (plugin->name() == name)
		{
			unregisterPlugin(plugin);

			auto m = pluginLibraries_.find(plugin);
			if (m != pluginLibraries_.end())
			{
				delete plugin;
				m->second.close();
				pluginLibraries_.erase(m);
				return;
			}
		}
	}
}

/** retrieves a list of currently loaded plugins */
std::vector<std::string> HttpServer::pluginsLoaded() const
{
	std::vector<std::string> result;

	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		result.push_back((*i)->name());

	return result;
}

HttpPlugin *HttpServer::registerPlugin(HttpPlugin *plugin)
{
	plugins_.push_back(plugin);

	return plugin;
}

HttpPlugin *HttpServer::unregisterPlugin(HttpPlugin *plugin)
{
	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i) {
		if (*i == plugin) {
			unregisterNative(plugin->name());
			plugins_.erase(i);
			break;
		}
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
