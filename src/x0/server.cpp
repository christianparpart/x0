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
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/strutils.hpp>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <cstdarg>
#include <cstdlib>

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
server::server(asio::io_service *io_service) :
	connection_open(),
	pre_process(),
	resolve_document_root(),
	resolve_entity(),
	generate_content(),
	post_process(),
	request_done(),
	connection_close(),
	listeners_(),
	io_service_ptr_(io_service ? 0 : new asio::io_service()),
	io_service_(io_service ? *io_service : *io_service_ptr_),
	active_(false),
	settings_(),
	logger_(),
	plugins_(),
	now_(),
	max_connections(512),
	max_keep_alive_requests(16),
	max_keep_alive_idle(5),
	max_read_idle(60),
	max_write_idle(360),
	tag("x0/" VERSION),
	fileinfo(io_service_),
	max_fds(boost::bind(&server::getrlimit, this, RLIMIT_CORE),
			boost::bind(&server::setrlimit, this, RLIMIT_NOFILE, _1))
{
	response::initialize();
}

server::~server()
{
	stop();
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

/**
 * configures the server ready to be started.
 */
void server::configure(const std::string& configfile)
{
	// load config
	settings_.load_file(configfile);

	settings_.load<std::string>("ServerTag", tag);

	// setup logger
	{
		std::string logmode(settings_.get<std::string>("Log.Mode"));
		auto nowfn = boost::bind(&datetime::htlog_str, &now_);

		if (logmode == "file") logger_.reset(new filelogger<decltype(nowfn)>(settings_.get<std::string>("Log.FileName"), nowfn));
		else if (logmode == "null") logger_.reset(new nulllogger());
		else if (logmode == "stderr") logger_.reset(new filelogger<decltype(nowfn)>("/dev/stderr", nowfn));
		/// \todo add syslog logger
		else logger_.reset(new nulllogger());
	}

	logger_->level(severity(settings_.get<std::string>("Log.Level")));

	// setup workers
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

	// resource limits
	{
		settings_.load("Resources.MaxConnections", max_connections);
		settings_.load("Resources.MaxKeepAliveRequests", max_keep_alive_requests);
		settings_.load("Resources.MaxKeepAliveIdle", max_keep_alive_idle);
		settings_.load("Resources.MaxReadIdle", max_read_idle);
		settings_.load("Resources.MaxWriteIdle", max_write_idle);

		long long value = 0;
		if (settings_.load("Resources.MaxFiles", value))
			setrlimit(RLIMIT_NOFILE, value);

		if (settings_.load("Resources.MaxAddressSpace", value))
			setrlimit(RLIMIT_AS, value);

		if (settings_.load("Resources.MaxCoreFileSize", value))
			setrlimit(RLIMIT_CORE, value);
	}

	// fileinfo
	{
		std::string value;
		if (settings_.load("FileInfo.MimeType.MimeFile", value))
			fileinfo.load_mimetypes(value);

		if (settings_.load("FileInfo.MimeType.DefaultType", value))
			fileinfo.default_mimetype(value);

		bool flag = false;
		if (settings_.load("FileInfo.ETag.ConsiderMtime", flag))
			fileinfo.etag_consider_mtime(flag);

		if (settings_.load("FileInfo.ETag.ConsiderSize", flag))
			fileinfo.etag_consider_size(flag);

		if (settings_.load("FileInfo.ETag.ConsiderInode", flag))
			fileinfo.etag_consider_inode(flag);
	}

	// load plugins
	{
		std::vector<std::string> plugins;
		settings_.load("Modules.Load", plugins);

		for (std::size_t i = 0; i < plugins.size(); ++i)
		{
			load_plugin(plugins[i]);
		}
	}

	// configure plugins
	for (plugin_map_t::iterator i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
	{
		i->second.first->configure();
	}

	// check for available TCP listeners
	if (listeners_.empty())
	{
		log(severity::critical, "No listeners defined. No virtual hosting plugin loaded or no virtual host defined?");
		throw std::runtime_error("No listeners defined. No virtual hosting plugin loaded or no virtual host defined?");
	}

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
	drop_privileges(settings_["Daemon.User"].as<std::string>(), settings_["Daemon.Group"].as<std::string>());
}

void server::start()
{
	if (!active_)
	{
		active_ = true;

		for (std::list<listener_ptr>::iterator i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
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

	io_service_.run();
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
	using boost::algorithm::ends_with;

	// update server clock
	now_.update();

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
	if (in->fileinfo->is_directory() && !ends_with(in->path, "/"))
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
listener_ptr server::listener_by_port(int port)
{
	for (std::list<listener_ptr>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		listener_ptr http_server = *k;

		if (http_server->port() == port)
		{
			return http_server;
		}
	}

	return listener_ptr();
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
		for (std::list<listener_ptr>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
		{
			(*k).reset();
		}

		io_service_.stop();
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
	vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	if (logger_)
	{
		logger_->write(s, buf);
	}
	else
	{
		std::fprintf(stderr, "%s\n", msg);
		std::fflush(stderr);
	}
}

void server::setup_listener(int port, const std::string& bind_address)
{
	// check if we already have an HTTP listener listening on given port
	if (listener_by_port(port))
		return;

	// create a new listener
	listener_ptr lp(new listener(*this));

	lp->configure(bind_address, port);

	listeners_.push_back(lp);
}

typedef plugin *(*plugin_create_t)(server&, const std::string&);

void server::load_plugin(const std::string& name)
{
	std::string plugindir_(".");
	settings_.load("Modules.Directory", plugindir_);

	if (!plugindir_.empty() && plugindir_[plugindir_.size() - 1] != '/')
	{
		plugindir_ += "/";
	}

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

} // namespace x0
