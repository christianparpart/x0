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
#include <cstdlib>
#include <pwd.h>
#include <grp.h>

// {{{ module hooks
extern "C" void accesslog_init(x0::server&);
extern "C" void dirlisting_init(x0::server&);
extern "C" void indexfile_init(x0::server&);
extern "C" void sendfile_init(x0::server&);
extern "C" void vhost_init(x0::server&);
// }}}

namespace x0 {

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
	logger_(),
	plugins_()
{
}

server::~server()
{
	stop();
}

/**
 * configures the server ready to be started.
 */
void server::configure()
{
	// load config
	config_.load_file("x0d.conf");

	debugging_ = config_.get("daemon", "debug") == "true";

	// setup error logger
	std::string logmode(config_.get("service", "errorlog-mode"));
	if (logmode == "file") logger_.reset(new filelogger(config_.get("service", "errorlog-filename")));
	else if (logmode == "null") logger_.reset(new nulllogger());
	else if (logmode == "stderr") logger_.reset(new filelogger("/dev/stderr"));
	else logger_.reset(new nulllogger());

	// load modules (currently builtin-only)
	accesslog_init(*this);
	dirlisting_init(*this);
	indexfile_init(*this);
	sendfile_init(*this);
	vhost_init(*this);

	// setup TCP listeners
	std::list<int> ports = split<int>(config_.get("service", "listen-ports"), ", ");
	for (std::list<int>::iterator i = ports.begin(), e = ports.end(); i != e; ++i)
	{
		setup_listener(*i);
	}

	// configure modules
	for (std::list<plugin_ptr>::iterator i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
	{
		(*i)->configure();
	}

	int nice_ = std::atoi(config_.get("daemon", "nice").c_str());
	if (nice_)
	{
		//LOG(*this, severity::debug, fstringbuilder::format("nice level: %d", nice_).c_str());
		LOG(*this, severity::debug, "nice level: %d", nice_);
		if (::nice(nice_) < 0)
		{
			throw std::runtime_error(fstringbuilder::format("could not nice process to %d: %s", nice_, strerror(errno)));
		}
	}

	drop_privileges(config_.get("daemon", "user"), config_.get("daemon", "group"));
	daemonize();
}

/**
 * starts the server
 */
void server::start()
{
	configure();

	paused_ = false;

	for (std::list<listener_ptr>::iterator i = listeners_.begin(); i != listeners_.end(); ++i)
	{
		(*i)->start();
	}
	LOG(*this, severity::info, "server up and running");
}

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

void server::daemonize()
{
	if (!debugging_)
	{
		if (::daemon(/*no chdir*/ true, /* no close */ true) < 0)
		{
			throw std::runtime_error(fstringbuilder::format("Could not daemonize process: %s", strerror(errno)));
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

	if (in.entity.size() > 3 && isdir(in.entity) && in.entity[in.entity.size() - 1] != '/')
	{
		// redirect physical request paths not ending with slash

		std::stringstream url;
		url << (in.secure ? "https://" : "http://") << in.get_header("Host") << in.path << '/' << in.query;

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

void server::stop()
{
	LOG(*this, severity::info, "server is shutting down");

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
	va_list va;
	va_start(va, msg);
	char buf[512];
	vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	if (s < severity::debug)		// do only print filename:line: prefix if we're at debug level
	{
		logger_->write(s, buf);
	}
	else if (debugging_)
	{
		logger_->write(s, fstringbuilder::format("%s:%d: %s", filename, line, buf));
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

void server::setup_plugin(plugin_ptr plug)
{
	plugins_.push_back(plug);
}

} // namespace x0
