/* <x0/server.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/config.hpp>
#include <x0/listener.hpp>
#include <x0/server.hpp>
#include <x0/strutils.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>

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

	// load modules (currently builtin-only)
	accesslog_init(*this);
	dirlisting_init(*this);
	indexfile_init(*this);
	sendfile_init(*this);
	vhost_init(*this);

	// setup TCP listeners
	auto ports = split<int>(config_.get("service", "listen-ports"), ", ");
	for (auto i = ports.begin(), e = ports.end(); i != e; ++i)
	{
		setup_listener(*i);
	}

	// configure modules
	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
	{
		(*i)->configure();
	}
}

/**
 * starts the server
 */
void server::start()
{
	configure();

	paused_ = false;

	for (auto i = listeners_.begin(); i != listeners_.end(); ++i)
	{
		(*i)->start();
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
	for (auto k = listeners_.begin(); k != listeners_.end(); ++k)
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
	for (auto k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		(*k)->stop();
	}
}

config& server::get_config()
{
	return config_;
}

void server::setup_listener(int port, const std::string& bind_address)
{
	// check if we already have an HTTP listener listening on given port
	if (listener_by_port(port))
		return;

	// create a new listener
	listener_ptr lp(new listener(io_service_, boost::bind(&server::handle_request, this, _1, _2)));

	lp->configure(bind_address, port);

	listeners_.insert(lp);
}

void server::setup_plugin(plugin_ptr plug)
{
	plugins_.push_back(plug);
}

} // namespace x0
