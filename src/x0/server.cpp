/* <x0/server.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/config.hpp>
#include <x0/logger.hpp>
#include <x0/listener.hpp>
#include <x0/server.hpp>
#include <x0/strutils.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <cstdlib>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <dlfcn.h>

// {{{ module hooks
extern "C" void accesslog_init(x0::server&);
extern "C" void dirlisting_init(x0::server&);
extern "C" void indexfile_init(x0::server&);
extern "C" void sendfile_init(x0::server&);
extern "C" void vhost_init(x0::server&);
// }}}

namespace x0 {

server *server::instance_ = 0;

/** concats a path with a filename and optionally inserts a path seperator if path 
 *  doesn't contain a trailing path seperator. */
static inline std::string pathcat(const std::string& path, const std::string& filename)
{
	if (!path.empty() && path[path.size() - 1] != '/')
	{
		return path + "/" + filename;
	}
	else
	{
		return path + filename;
	}
}

server::server(boost::asio::io_service& io_service) :
	connection_open(),
	pre_process(),
	resolve_document_root(),
	resolve_entity(),
	generate_content(),
	request_done(),
	post_process(),
	connection_close(),
	listeners_(),
	io_service_(io_service),
	paused_(),
	config_(),
	configfile_(pathcat(SYSCONFDIR, "x0d.conf")),
	nofork_(),
	logger_(),
	plugins_()
{
	instance_ = this;
}

server::~server()
{
	stop();

	instance_ = 0;
}

/**
 * configures the server ready to be started.
 */
void server::configure()
{
	// load config
	config_.load_file(configfile_);

	// setup logger
	std::string logmode(config_.get("service", "log-mode"));
	if (logmode == "file") logger_.reset(new filelogger(config_.get("service", "log-filename")));
	else if (logmode == "null") logger_.reset(new nulllogger());
	else if (logmode == "stderr") logger_.reset(new filelogger("/dev/stderr"));
	else logger_.reset(new nulllogger());

	logger_->level(severity(config_.get("service", "log-level")));

	// load modules
	std::vector<std::string> plugins = split<std::string>(config_.get("service", "modules-load"), ", ");

	for (std::size_t i = 0; i < plugins.size(); ++i)
	{
		load_plugin(plugins[i]);
	}

	// setup TCP listeners
	std::vector<int> ports = split<int>(config_.get("service", "listen-ports"), ", ");
	for (std::vector<int>::iterator i = ports.begin(), e = ports.end(); i != e; ++i)
	{
		setup_listener(*i);
	}

	// configure modules
	for (plugin_map_t::iterator i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
	{
		i->second.first->configure();
	}

	// setup process priority
	if (int nice_ = std::atoi(config_.get("daemon", "nice").c_str()))
	{
		LOG(*this, severity::debug, "set nice level to %d", nice_);

		if (::nice(nice_) < 0)
		{
			throw std::runtime_error(fstringbuilder::format("could not nice process to %d: %s", nice_, strerror(errno)));
		}
	}

	// drop user privileges
	drop_privileges(config_.get("daemon", "user"), config_.get("daemon", "group"));
}

/**
 * parses command line arguments.
 */
bool server::parse(int argc, char *argv[])
{
	struct option long_options[] =
	{
		{ "no-fork", no_argument, &nofork_, 1 },
		{ "fork", no_argument, &nofork_, 0 },
		//.
		{ "version", no_argument, 0, 'v' },
		{ "copyright", no_argument, 0, 'y' },
		{ "config", required_argument, 0, 'c' },
		{ "help", no_argument, 0, 'h' },
		//.
		{ 0, 0, 0, 0 }
	};

	static const char *package_header = 
		"x0d: x0 web server, version " PACKAGE_VERSION " [" PACKAGE_HOMEPAGE_URL "]";
	static const char *package_copyright =
		"Copyright (c) 2009 by Christian Parpart <trapni@gentoo.org>";
	static const char *package_license =
		"Licensed under GPL-3 [http://gplv3.fsf.org/]";

	nofork_ = 0;

	for (;;)
	{
		int long_index = 0;
		switch (getopt_long(argc, argv, "vyc:hX", long_options, &long_index))
		{
			case 'c':
				configfile_ = optarg;
				break;
			case 'v':
				std::cout
					<< package_header << std::endl
					<< package_copyright << std::endl;
			case 'y':
				std::cout << package_license << std::endl;
				return false;
			case 'h':
				std::cout
					<< package_header << std::endl
					<< package_copyright << std::endl
					<< package_license << std::endl
					<< std::endl
					<< "usage:" << std::endl
					<< "   x0d [options ...]" << std::endl
					<< std::endl
					<< "options:" << std::endl
					<< "   -h,--help        print this help" << std::endl
					<< "   -c,--config=PATH specify a custom configuration file" << std::endl
					<< "   -X,--no-fork     do not fork into background" << std::endl
					<< "   -v,--version     print software version" << std::endl
					<< "   -y,--copyright   print software copyright notice / license" << std::endl
					<< std::endl;
				return false;
			case 'X':
				nofork_ = true;
				break;
			case 0: // long option with (val!=NULL && flag=0)
				break;
			case -1: // EOF - everything parsed.
				return true;
			case '?': // ambiguous match / unknown arg
			default:
				return false;
		}
	}
}

/**
 * starts the server
 */
void server::start(int argc, char *argv[])
{
	if (parse(argc, argv))
	{
		configure();

		paused_ = false;

		for (std::list<listener_ptr>::iterator i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
		{
			(*i)->start();
		}

		if (!nofork_)
		{
			daemonize();
		}

		::signal(SIGHUP, &reload_handler);
		::signal(SIGTERM, &terminate_handler);

		LOG(*this, severity::info, "server up and running");
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
}

/** forks server process into background. */
void server::daemonize()
{
	if (::daemon(/*no chdir*/ true, /* no close */ true) < 0)
	{
		throw std::runtime_error(fstringbuilder::format("Could not daemonize process: %s", strerror(errno)));
	}
}

void server::handle_request(request& in, response& out) {
	// pre-request hook
	pre_process(in);

	// resolve document root
	resolve_document_root(in);

	if (in.document_root.empty())
	{
		// no document root assigned with this request.
		in.document_root = "/dev/null";
	}

	// resolve entity
	in.entity = in.document_root + in.path;
	resolve_entity(in);

	// redirect physical request paths not ending with slash
	if (in.entity.size() > 3 && isdir(in.entity) && in.entity[in.entity.size() - 1] != '/')
	{
		std::stringstream url;
		url << (in.secure ? "https://" : "http://") << in.header("Host") << in.path << '/' << in.query;

		out.status = response::moved_permanently->status;
		out.content = response::moved_permanently->content;

		out *= header("Location", url.str());
		out *= header("Content-Type", "text/html");
	}
	// generate response content, based on this request
	else if (!generate_content(in, out))
	{
		// no content generator found for this request, default to 404 (Not Found)
		out.status = response::not_found->status;
		out.content = response::not_found->content;
		out *= header("Content-Type", "text/html");
	}

	if (!out.status)
	{
		// content generator found, but no status set, default to 200 (Ok)
		out.status = 200;
	}

	if (!out.has_header("Content-Length"))
	{
		out += header("Content-Length", boost::lexical_cast<std::string>(out.content.length()));
	}

	if (!out.has_header("Content-Type"))
	{
		out += header("Content-Type", "text/plain");
	}

	// log request/response
	request_done(in, out);

	// post-response hook
	post_process(in, out);
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
	paused_ = true;
}

void server::resume()
{
	paused_ = false;
}

void server::reload()
{
	// TODO
}

void server::stop()
{
	for (std::list<listener_ptr>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		(*k)->stop();
	}
}

config& server::get_config()
{
	return config_;
}

void server::log(const char *filename, unsigned int line, severity s, const char *msg, ...)
{
	if (logger_)
	{
		va_list va;
		va_start(va, msg);
		char buf[512];
		vsnprintf(buf, sizeof(buf), msg, va);
		va_end(va);

		if (s < severity::debug)		// do only print filename:line: prefix if we're at debug level
		{
			logger_->write(s, buf);
		}
		else
		{
			logger_->write(s, fstringbuilder::format("%s:%d: %s", filename, line, buf));
		}
	}
}

void server::reload_handler(int)
{
	if (instance_)
	{
		LOG(*instance_, severity::info, "SIGHUP received. Reloading configuration.");

		try
		{
			instance_->reload();
		}
		catch (std::exception& e)
		{
			LOG(*instance_, severity::error, "uncaught exception in reload handler: %s", e.what());
		}
	}
}

void server::terminate_handler(int)
{
	if (instance_)
	{
		LOG(*instance_, severity::info, "SIGTERM received. Shutting down.");

		try
		{
			instance_->stop();
		}
		catch (std::exception& e)
		{
			LOG(*instance_, severity::error, "uncaught exception in terminate handler: %s", e.what());
		}
	}
}

void server::setup_listener(int port, const std::string& bind_address)
{
	// check if we already have an HTTP listener listening on given port
	if (listener_by_port(port))
		return;

	// create a new listener
	listener_ptr lp(new listener(io_service_, boost::bind(&server::handle_request, this, _1, _2)));

	lp->configure(bind_address, port);

	listeners_.push_back(lp);
}

typedef plugin *(*plugin_create_t)(server&, const std::string&);

void server::load_plugin(const std::string& name)
{
	std::string plugindir_(config_.get("service", "modules-directory"));

	if (!plugindir_.empty() && plugindir_[plugindir_.size() - 1] != '/')
	{
		plugindir_ += "/";
	}

	std::string filename(fstringbuilder::format("%s%s.so", plugindir_.c_str(), name.c_str()));
	std::string plugin_create_name(fstringbuilder::format("%s_init", name.c_str()));

	LOG(*this, severity::debug, "Loading plugin %s", filename.c_str());
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


















