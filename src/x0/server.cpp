/* <x0/server.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/settings.hpp>
#include <x0/logger.hpp>
#include <x0/listener.hpp>
#include <x0/server.hpp>
#include <x0/plugin.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/strutils.hpp>
#include <x0/ansi_color.hpp>
#include <x0/sysconfig.h>

#include <iostream>
#include <cstdarg>
#include <cstdlib>

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#	include <gnutls/gnutls.h>
#	include <gnutls/extra.h>
#	include <pthread.h>
#	include <gcrypt.h>
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif

#if defined(HAVE_SYS_UTSNAME_H)
#	include <sys/utsname.h>
#endif

#include <sys/resource.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <dlfcn.h>
#include <getopt.h>

namespace x0 {

/** initializes the HTTP server object.
 * \param io_service an Asio io_service to use or NULL to create our own one.
 * \see server::run()
 */
server::server(struct ::ev_loop *loop) :
	connection_open(),
	pre_process(),
	resolve_document_root(),
	resolve_entity(),
	generate_content(),
	post_process(),
	request_done(),
	connection_close(),
	listeners_(),
	loop_(loop ? loop : ev_default_loop(0)),
	active_(false),
	settings_(),
	cvars_server_(),
	cvars_host_(),
	cvars_path_(),
	logger_(),
	colored_log_(false),
	plugins_(),
	now_(),
	loop_check_(loop_),
	max_connections(512),
	max_keep_alive_requests(16),
	max_keep_alive_idle(5),
	max_read_idle(60),
	max_write_idle(360),
	tag("x0/" VERSION),
	fileinfo(loop_),
	max_fds(std::bind(&server::getrlimit, this, RLIMIT_CORE),
			std::bind(&server::setrlimit, this, RLIMIT_NOFILE, std::placeholders::_1))
{
	response::initialize();

	// initialize all cvar maps with all (valid) priorities
	for (int i = -10; i <= +10; ++i)
	{
		cvars_server_[i].clear();
		cvars_host_[i].clear();
		cvars_path_[i].clear();
	}

	loop_check_.set<server, &server::loop_check>(this);
	loop_check_.start();

	// register cvars
	using namespace std::placeholders;
	register_cvar_server("Log", std::bind(&server::setup_logging, this, _1), -7);
	register_cvar_server("Resources", std::bind(&server::setup_resources, this, _1), -6);
	register_cvar_server("Modules", std::bind(&server::setup_modules, this, _1), -5);
	register_cvar_server("ErrorDocuments", std::bind(&server::setup_error_documents, this, _1), -4);
	register_cvar_server("FileInfo", std::bind(&server::setup_fileinfo, this, _1), -4);
	register_cvar_server("Hosts", std::bind(&server::setup_hosts, this, _1), -3);

#if defined(WITH_SSL)
	gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

	int rv = gnutls_global_init();
	if (rv != GNUTLS_E_SUCCESS)
		throw std::runtime_error("could not initialize gnutls library");

	gnutls_global_init_extra();
#endif
}

void server::loop_check(ev::check& /*w*/, int /*revents*/)
{
	// update server time
	now_.update(static_cast<time_t>(ev_now(loop_)));
}

#if defined(WITH_SSL)
void server::gnutls_log(int level, const char *msg)
{
	fprintf(stderr, "gnutls log[%d]: %s", level, msg);
	fflush(stderr);
}
#endif

server::~server()
{
	stop();

	for (std::list<listener *>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
		delete *k;
}

static inline const char *rc2str(int resource)
{
	switch (resource)
	{
		case RLIMIT_CORE: return "core";
		case RLIMIT_AS: return "address-space";
		case RLIMIT_NOFILE: return "filedes";
		default: return "unknown";
	}
}

long long server::getrlimit(int resource)
{
	struct rlimit rlim;
	if (::getrlimit(resource, &rlim) == -1)
	{
		log(severity::warn, "Failed to retrieve current resource limit on %s (%d).",
			rc2str(resource), resource);
		return 0;
	}
	return rlim.rlim_cur;
}

long long server::setrlimit(int resource, long long value)
{
	struct rlimit rlim;
	if (::getrlimit(resource, &rlim) == -1)
	{
		log(severity::warn, "Failed to retrieve current resource limit on %s (%d).",
			rc2str(resource), resource);

		return 0;
	}

	long long last = rlim.rlim_cur;

	// patch against human readable form
	long long hlast = last, hvalue = value;
	switch (resource)
	{
		case RLIMIT_AS:
		case RLIMIT_CORE:
			hlast /= 1024 / 1024;
			value *= 1024 * 1024;
			break;
		default:
			break;
	}

	rlim.rlim_cur = value;
	rlim.rlim_max = value;

	if (::setrlimit(resource, &rlim) == -1) {
		log(severity::warn, "Failed to set resource limit on %s (%d) from %lld to %lld.",
			rc2str(resource), resource, hlast, hvalue);

		return 0;
	}

	log(severity::debug, "Set resource limit on %s (%d) from %lld to %lld.",
		rc2str(resource), resource, hlast, hvalue);

	return value;
}

template<typename K, typename V>
inline bool contains(const std::map<K, V>& map, const K& key)
{
	for (auto i = map.begin(), e = map.end(); i != e; ++i)
		if (i->first == key)
			return true;

	return false;
}

inline bool contains(const std::map<int, std::map<std::string, std::function<void(const settings_value&)>>>& map, const std::string& cvar)
{
	for (auto pi = map.begin(), pe = map.end(); pi != pe; ++pi)
		for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
			if (ci->first == cvar)
				return true;

	return false;
}

inline bool contains(const std::map<int, std::map<std::string, std::function<void(const settings_value&, const std::string& hostid)>>>& map, const std::string& cvar)
{
	for (auto pi = map.begin(), pe = map.end(); pi != pe; ++pi)
		for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
			if (ci->first == cvar)
				return true;

	return false;
}

inline bool contains(const std::map<int, std::map<std::string, std::function<void(const settings_value&, const std::string& hostid, const std::string& path)>>>& map, const std::string& cvar)
{
	for (auto pi = map.begin(), pe = map.end(); pi != pe; ++pi)
		for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
			if (ci->first == cvar)
				return true;

	return false;
}

inline bool contains(const std::vector<std::string>& list, const std::string& var)
{
	for (auto i = list.begin(), e = list.end(); i != e; ++i)
		if (*i == var)
			return true;

	return false;
}

/**
 * configures the server ready to be started.
 */
void server::configure(const std::string& configfile)
{
	std::vector<std::string> global_ignores = {
		"IGNORES",
		"string", "xpcall", "package", "io", "coroutine", "collectgarbage", "getmetatable", "module",
		"loadstring", "rawget", "rawset", "ipairs", "pairs", "_G", "next", "assert", "tonumber",
		"rawequal", "tostring", "print", "os", "unpack", "gcinfo", "require", "getfenv", "setmetatable",
		"type", "newproxy", "table", "pcall", "math", "debug", "select", "_VERSION",
		"dofile", "setfenv", "load", "error", "loadfile"
	};

	// load config
	settings_.load_file(configfile);

	// {{{ global vars
	auto globals = settings_.keys();
	auto custom_ignores = settings_["IGNORES"].values<std::string>();

	// iterate all server cvars
	for (auto pi = cvars_server_.begin(), pe = cvars_server_.end(); pi != pe; ++pi)
		for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
			if (contains(globals, ci->first))
				ci->second(settings_[ci->first]);

	// warn on every unknown global cvar
	for (auto i = globals.begin(), e = globals.end(); i != e; ++i)
	{
		if (contains(global_ignores, *i))
			continue;
		if (contains(custom_ignores, *i))
			continue;
		else if (!contains(cvars_server_, *i))
			log(severity::warn, "Unknown global configuration variable: '%s'.", i->c_str());
	}
	// }}}

	// {{{ setup server-tag
	{
		std::vector<std::string> components;

		settings_.load<std::vector<std::string>>("ServerTags", components);

		//! \todo add zlib version
		//! \todo add bzip2 version

#if defined(WITH_SSL)
		components.insert(components.begin(), std::string("GnuTLS/") + gnutls_check_version(NULL));
#endif

#if defined(HAVE_SYS_UTSNAME_H)
		{
			utsname utsname;
			if (uname(&utsname) == 0)
			{
				components.insert(components.begin(), 
					std::string(utsname.sysname) + "/" + utsname.release
				);

				components.insert(components.begin(), utsname.machine);
			}
		}
#endif

		buffer tagbuf;
		tagbuf.push_back("x0/" VERSION);

		if (!components.empty())
		{
			tagbuf.push_back(" (");

			for (int i = 0, e = components.size(); i != e; ++i)
			{
				if (i)
					tagbuf.push_back(", ");

				tagbuf.push_back(components[i]);
			}

			tagbuf.push_back(")");
		}
		tag = tagbuf.str();
	}
	// }}}

#if defined(WITH_SSL)
	// gnutls debug level (int, 0=off)
//	gnutls_global_set_log_level(10);
//	gnutls_global_set_log_function(&server::gnutls_log);
#endif

	// {{{ setup workers
#if 0
	{
		int num_workers = 1;
		settings_.load("Resources.NumWorkers", num_workers);
		//io_service_pool_.setup(num_workers);

		if (num_workers > 1)
			log(severity::info, "using %d workers", num_workers);
		else
			log(severity::info, "using single worker");
	}
#endif
	// }}}

	// check for available TCP listeners
	if (listeners_.empty())
	{
		log(severity::critical, "No listeners defined. No virtual hosting plugin loaded or no virtual host defined?");
		throw std::runtime_error("No listeners defined. No virtual hosting plugin loaded or no virtual host defined?");
	}

	for (std::list<listener *>::iterator i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
		(*i)->prepare();

	// setup process priority
	if (int nice_ = settings_.get<int>("Daemon.Nice"))
	{
		log(severity::debug, "set nice level to %d", nice_);

		if (::nice(nice_) < 0)
		{
			throw std::runtime_error(fstringbuilder::format("could not nice process to %d: %s", nice_, strerror(errno)));
		}
	}

	// drop user privileges
	drop_privileges(settings_["Daemon"]["User"].as<std::string>(), settings_["Daemon"]["Group"].as<std::string>());
}

void server::start()
{
	if (!active_)
	{
		active_ = true;

		for (std::list<listener *>::iterator i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
		{
			(*i)->start();
		}
	}
}

/** tests whether this server has been started or not.
 * \see start(), run()
 */
bool server::active() const
{
	return active_;
}

/** calls run on the internally referenced io_service.
 * \note use this if you do not have your own main loop.
 * \note automatically starts the server if it wasn't started via \p start() yet.
 */
void server::run()
{
	if (!active_)
		start();

	while (active_)
	{
		ev_loop(loop_, 0);
	}
}

/** drops runtime privileges current process to given user's/group's name. */
void server::drop_privileges(const std::string& username, const std::string& groupname)
{
	if (!groupname.empty() && !getgid())
	{
		if (struct group *gr = getgrnam(groupname.c_str()))
		{
			if (setgid(gr->gr_gid) != 0)
			{
				throw std::runtime_error(fstringbuilder::format("could not setgid to %s: %s", groupname.c_str(), strerror(errno)));
			}
		}
		else
		{
			throw std::runtime_error(fstringbuilder::format("Could not find group: %s", groupname.c_str()));
		}
	}

	if (!username.empty() && !getuid())
	{
		if (struct passwd *pw = getpwnam(username.c_str()))
		{
			if (setuid(pw->pw_uid) != 0)
			{
				throw std::runtime_error(fstringbuilder::format("could not setgid to %s: %s", username.c_str(), strerror(errno)));
			}

			if (chdir(pw->pw_dir) < 0)
			{
				throw std::runtime_error(fstringbuilder::format("could not chdir to %s: %s", pw->pw_dir, strerror(errno)));
			}
		}
		else
		{
			throw std::runtime_error(fstringbuilder::format("Could not find group: %s", groupname.c_str()));
		}
	}

	if (!::getuid() || !::geteuid() || !::getgid() || !::getegid())
	{
#if defined(X0_RELEASE)
		throw std::runtime_error(fstringbuilder::format("Service is not allowed to run with administrative permissionsService is still running with administrative permissions."));
#else
		log(severity::warn, "Service is still running with administrative permissions.");
#endif
	}
}

void server::handle_request(request *in, response *out)
{
	// pre-request hook
	pre_process(const_cast<x0::request *>(in));

	// resolve document root
	resolve_document_root(const_cast<x0::request *>(in));

	if (in->document_root.empty())
	{
		// no document root assigned with this request.
		// -> make sure it is not exploited.
		in->document_root = "/dev/null";
	}

	// resolve entity
	in->fileinfo = fileinfo(in->document_root + in->path);
	resolve_entity(const_cast<x0::request *>(in)); // translate_path

	// redirect physical request paths not ending with slash if mapped to directory
	std::string filename = in->fileinfo->filename();
	if (in->fileinfo->is_directory() && !in->path.ends('/'))
	{
		std::stringstream url;

		buffer_ref hostname(in->header("X-Forwarded-Host"));
		if (hostname.empty())
			hostname = in->header("Host");

		url << (in->connection.secure ? "https://" : "http://");
		url << hostname.str();
		url << in->path.str();
		url << '/' << in->query;

		//*out *= response_header("Location", url.str());
		out->headers.set("Location", url.str());
		out->status = response::moved_permanently;

		out->finish();
		return;
	}

	// generate response content, based on this request
	generate_content(std::bind(&response::finish, out), const_cast<x0::request *>(in), const_cast<x0::response *>(out));
}

/**
 * retrieves the listener object that is responsible for the given port number, or null otherwise.
 */
listener *server::listener_by_port(int port)
{
	for (std::list<listener *>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		listener *http_server = *k;

		if (http_server->port() == port)
		{
			return http_server;
		}
	}

	return 0;
}

void server::pause()
{
	active_ = false;
}

void server::resume()
{
	active_ = true;
}

void server::reload()
{
	//! \todo implementation
}

/** unregisters all listeners from the underlying io_service and calls stop on it.
 * \see start(), active(), run()
 */
void server::stop()
{
	if (active_)
	{
		active_ = false;

		for (std::list<listener *>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
		{
			(*k)->stop();
		}

		ev_unloop(loop_, ev::ALL);
	}
}

x0::settings& server::config()
{
	return settings_;
}

void server::log(severity s, const char *msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buf[512];
	int buflen = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	if (colored_log_)
	{
		static ansi_color::color_type colors[] = {
			ansi_color::ccRed, // emergency
			ansi_color::ccRed | ansi_color::ccBold, // alert
			ansi_color::ccRed, // critical
			ansi_color::ccRed | ansi_color::ccBold, // error
			ansi_color::ccYellow | ansi_color::ccBold, // warn
			ansi_color::ccWhite | ansi_color::ccBold, // notice
			ansi_color::ccGreen, // info
			ansi_color::ccCyan, // debug
		};

		buffer sb;
		sb.push_back(ansi_color::make(colors[s]));
		sb.push_back(buf, buflen);
		sb.push_back(ansi_color::make(ansi_color::ccClear));

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

listener *server::setup_listener(int port, const std::string& bind_address)
{
	// check if we already have an HTTP listener listening on given port
	if (listener *lp = listener_by_port(port))
		return lp;

	// create a new listener
	listener *lp = new listener(*this);

	lp->address(bind_address);
	lp->port(port);

	int value = 0;
	if (settings_.load("Resources.MaxConnections", value))
		lp->backlog(value);

	listeners_.push_back(lp);

	return lp;
}

typedef plugin *(*plugin_create_t)(server&, const std::string&);

void server::load_plugin(const std::string& name)
{
	std::string plugindir_(".");
	settings_.load("Modules.Directory", plugindir_);

	if (!plugindir_.empty() && plugindir_[plugindir_.size() - 1] != '/')
		plugindir_ += "/";

	std::string filename(plugindir_ + name + ".so");
	std::string plugin_create_name(name + "_init");

	log(severity::debug, "Loading plugin %s", filename.c_str());

	if (void *handle = dlopen(filename.c_str(), RTLD_GLOBAL | RTLD_NOW))
	{
		plugin_create_t plugin_create = reinterpret_cast<plugin_create_t>(dlsym(handle, plugin_create_name.c_str()));

		if (plugin_create)
		{
			plugin_ptr plugin = plugin_ptr(plugin_create(*this, name));
			plugins_[name] = plugin_value_t(plugin, handle);
		}
		else
		{
			std::string msg(fstringbuilder::format("error loading plugin '%s' %s", name.c_str(), dlerror()));
			dlclose(handle);
			throw std::runtime_error(msg);
		}
	}
	else
	{
		throw std::runtime_error(fstringbuilder::format("Cannot load plugin '%s'. %s", name.c_str(), dlerror()));
	}
}

void server::unload_plugin(const std::string& name)
{
	plugin_map_t::iterator i = plugins_.find(name);

	if (i != plugins_.end())
	{
		// clear ptr to local map, though, deallocating this plugin object
		i->second.first = plugin_ptr();

		// close system handle
		dlclose(i->second.second);

		plugins_.erase(i);
	}
}

std::vector<std::string> server::loaded_plugins() const
{
	std::vector<std::string> result;

	for (plugin_map_t::const_iterator i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		result.push_back(i->first);

	return result;
}

bool server::register_cvar_server(const std::string& key, std::function<void(const settings_value&)> callback, int priority)
{
	priority = std::min(std::max(priority, -10), 10);
	cvars_server_[priority][key] = callback;
	return true;
}

bool server::register_cvar_host(const std::string& key, std::function<void(const settings_value&, const std::string& hostid)> callback, int priority)
{
	priority = std::min(std::max(priority, -10), 10);
	cvars_host_[priority][key] = callback;
	return true;
}

bool server::register_cvar_path(const std::string& key, std::function<void(const settings_value&, const std::string& hostid, const std::string& path)> callback, int priority)
{
	priority = std::min(std::max(priority, -10), 10);
	cvars_path_[priority][key] = callback;
	return true;
}

void server::setup_logging(const settings_value& cvar)
{
	std::string logmode(cvar["Mode"].as<std::string>());
	auto nowfn = std::bind(&datetime::htlog_str, &now_);

	if (logmode == "file")
		logger_.reset(new filelogger<decltype(nowfn)>(cvar["FileName"].as<std::string>(), nowfn));
	else if (logmode == "null")
		logger_.reset(new nulllogger());
	else if (logmode == "stderr")
		logger_.reset(new filelogger<decltype(nowfn)>("/dev/stderr", nowfn));
	else //! \todo add syslog logger
		logger_.reset(new nulllogger());

	logger_->level(severity(cvar["Level"].as<std::string>()));

	cvar["Colorize"].load(colored_log_);
}

void server::setup_modules(const settings_value& cvar)
{
	std::vector<std::string> list;
	cvar["Load"].load(list);

	for (auto i = list.begin(), e = list.end(); i != e; ++i)
		load_plugin(*i);

	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		i->second.first->configure();
}

void server::setup_resources(const settings_value& cvar)
{
	cvar["MaxConnections"].load(max_connections);
	cvar["MaxKeepAliveRequests"].load(max_keep_alive_requests);
	cvar["MaxKeepAliveIdle"].load(max_keep_alive_idle);
	cvar["MaxReadIdle"].load(max_read_idle);
	cvar["MaxWriteIdle"].load(max_write_idle);

	long long value = 0;
	if (cvar["MaxFiles"].load(value))
		setrlimit(RLIMIT_NOFILE, value);

	if (cvar["MaxAddressSpace"].load(value))
		setrlimit(RLIMIT_AS, value);

	if (cvar["MaxCoreFileSize"].load(value))
		setrlimit(RLIMIT_CORE, value);
}

void server::setup_hosts(const settings_value& cvar)
{
	std::vector<std::string> hostids = cvar.keys<std::string>();

	for (auto i = hostids.begin(), e = hostids.end(); i != e; ++i)
	{
		std::string hostid = *i;

		auto host_cvars = cvar[hostid].keys<std::string>();

		// handle all vhost-directives
		for (auto pi = cvars_host_.begin(), pe = cvars_host_.end(); pi != pe; ++pi)
		{
			for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
			{
				if (contains(host_cvars, ci->first))
				{
					DEBUG("CVAR_HOST(%s): %s", hostid.c_str(), ci->first.c_str());
					ci->second(cvar[hostid][ci->first], hostid);
				}
			}
		}

		// handle all path scopes
		for (auto vi = host_cvars.begin(), ve = host_cvars.end(); vi != ve; ++vi)
		{
			std::string path = *vi;
			if (path[0] == '/')
			{
				std::vector<std::string> keys = cvar[hostid][path].keys<std::string>();

				for (auto pi = cvars_path_.begin(), pe = cvars_path_.end(); pi != pe; ++pi)
					for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
						if (contains(keys, ci->first))
							ci->second(cvar[hostid][path], hostid, path);

				for (auto ki = keys.begin(), ke = keys.end(); ki != ke; ++ki)
					if (!contains(cvars_path_, *ki))
						log(severity::error, "Unknown location-context variable: '%s'", ki->c_str());
			}
		}
	}
}

void server::setup_fileinfo(const settings_value& cvar)
{
	std::string value;
	if (cvar["MimeType"]["MimeFile"].load(value))
		fileinfo.load_mimetypes(value);

	if (cvar["MimeType"]["DefaultType"].load(value))
		fileinfo.default_mimetype(value);

	bool flag = false;
	if (cvar["ETag"]["ConsiderMtime"].load(flag))
		fileinfo.etag_consider_mtime(flag);

	if (cvar["ETag"]["ConsiderSize"].load(flag))
		fileinfo.etag_consider_size(flag);

	if (cvar["ETag"]["ConsiderInode"].load(flag))
		fileinfo.etag_consider_inode(flag);
}

void server::setup_error_documents(const settings_value& cvar)
{
}

} // namespace x0
