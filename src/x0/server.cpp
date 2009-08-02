/* <x0/server.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/config.hpp>
#include <x0/logger.hpp>
#include <x0/listener.hpp>
#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/strutils.hpp>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <cstdarg>
#include <cstdlib>

#include <pwd.h>
#include <grp.h>
#include <dlfcn.h>
#include <getopt.h>

namespace x0 {

server::server() :
	connection_open(),
	pre_process(),
	resolve_document_root(),
	resolve_entity(),
	generate_content(),
	request_done(),
	post_process(),
	connection_close(),
	listeners_(),
	io_service_pool_(),
	paused_(),
	config_(),
	logger_(),
	plugins_(),
	max_connections(512),
	max_fds(1024),
	max_keep_alive_requests(16),
	max_keep_alive_idle(5),
	max_read_idle(60),
	max_write_idle(360),
	tag("x0/" VERSION),
	stat(io_service_pool_.get_service())
{
}

server::~server()
{
	stop();
}

/**
 * configures the server ready to be started.
 */
void server::configure(const std::string& configfile)
{
	// load config
	config_.load_file(configfile);

	// setup logger
	std::string logmode(config_.get("service", "log-mode"));
	if (logmode == "file") logger_.reset(new filelogger(config_.get("service", "log-filename")));
	else if (logmode == "null") logger_.reset(new nulllogger());
	else if (logmode == "stderr") logger_.reset(new filelogger("/dev/stderr"));
	else logger_.reset(new nulllogger());

	logger_->level(severity(config_.get("service", "log-level")));

	// setup io_service_pool
	int num_threads = 1;
	config_.load<int>("service", "num-threads", num_threads);
	io_service_pool_.setup(num_threads);

	if (num_threads > 1)
		LOG(*this, severity::info, "using %d io services", num_threads);
	else
		LOG(*this, severity::info, "using single io service");

	// load limits
	config_.load<int>("service", "max-connections", max_connections);
	config_.load<int>("service", "max-fds", max_fds);
	config_.load<int>("service", "max-keep-alive-requests", max_keep_alive_requests);
	config_.load<int>("service", "max-keep-alive-idle", max_keep_alive_idle);
	config_.load<int>("service", "max-read-idle", max_read_idle);
	config_.load<int>("service", "max-write-idle", max_write_idle);

	config_.load<std::string>("service", "tag", tag);

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
 * run the server
 */
void server::run()
{
	paused_ = false;

	for (std::list<listener_ptr>::iterator i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
	{
		(*i)->start();
	}

	LOG(*this, severity::info, "server up and running");

	io_service_pool_.run();
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
	struct stat *st = stat(in.entity);
	if (st && S_ISDIR(st->st_mode) && in.entity.size() > 3 && in.entity[in.entity.size() - 1] != '/')
	{
		std::stringstream url;
		url << (in.connection.secure ? "https://" : "http://") << in.header("Host") << in.path << '/' << in.query;

		out *= header("Location", url.str());
		out.status = response::moved_permanently;

		out.flush();
		return;
	}

	// generate response content, based on this request
	bool rv = generate_content(in, out);//, boost::bind(server::generated, this, in, out));
	if (!rv)
	{
		// no content generator found for this request, default to 404 (Not Found)
		out.status = response::not_found;

		out.flush();
	}
}
#if 0
	// log request/response
	request_done(in, out);

	// post-response hook
	post_process(in, out);
}
#endif

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
		(*k).reset();
	}

	io_service_pool_.stop();
}

x0::config& server::config()
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
	std::string plugindir_(config_.get("service", "modules-directory"));

	if (!plugindir_.empty() && plugindir_[plugindir_.size() - 1] != '/')
	{
		plugindir_ += "/";
	}

	std::string filename(plugindir_ + name + ".so");
	std::string plugin_create_name(name + "_init");

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


















