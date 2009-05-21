#include <x0/server.hpp>
#include <x0/config.hpp>
#include <x0/listener.hpp>
#include <x0/request_handler.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>

namespace x0 {

server::server(io_service& io_service) :
	pre_processor(),
	uri_mapper(),
	uri_validator(),
	document_root_resolver(),
	content_generator(),
	response_header_generator(),
	response_header_validator(),
	connection_logger(),
	post_processor(),
	listeners_(),
	io_service_(io_service),
	paused_(),
	config_(),
	plugins_()
{
	response_header_generator.connect(bind(&server::generate_response_headers, this, _1, _2));
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

	// load modules
	vhost_init(*this);
	sendfile_init(*this);
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
	pre_processor(in);
	uri_mapper(in);
	uri_validator(in);

	document_root_resolver(in);
	if (in.document_root.empty())
	{
		// no document root assigned with this request.
		in.document_root = "/dev/null";
	}

	if (!content_generator(in, out))
	{
		// no content generator found for this request, default to 404 (Not Found)
		out.content = response::not_found->status;
		out.content = response::not_found->content;
	}
	else if (!out.status)
	{
		// content generator found, but no status set, default to 200 (Ok)
		out.status = 200;
	}

	response_header_generator(in, out);
	response_header_validator(in, out);
	connection_logger(in, out);
	post_processor(in, out);
}

void server::generate_response_headers(request& in, response& out)
{
	if (!out.has_header("Content-Length"))
	{
		out += header("Content-Length", lexical_cast<std::string>(out.content.length()));
	}

	if (!out.has_header("Content-Type"))
	{
		out += header("Content-Type", "text/plain");
	}
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
	listener_ptr lp(new listener(io_service_, bind(&server::handle_request, this, _1, _2)));

	lp->configure(bind_address, port);

	listeners_.insert(lp);
}

void server::setup_plugin(plugin_ptr plug)
{
	plugins_.push_back(plug);
}

} // namespace x0
