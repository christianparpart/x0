/* <x0/mod_cgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/strutils.hpp>
#include <x0/process.hpp>
#include <x0/types.hpp>

#include <boost/enable_shared_from_this.hpp>
#include <boost/system/error_code.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/tokenizer.hpp>
#include <boost/signal.hpp>
#include <boost/bind.hpp>

#include <algorithm>
#include <string>
#include <cctype>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * POSSIBLE CONFIGURATION SETTINGS
 *
 * [cgi]
 * ttl = 5                  ; max time in seconds a cgi may run until SIGTERM is issued (-1 for unlimited).
 * kill-ttl = 5             ; max time to wait from SIGTERM on before a SIGKILL is ussued (-1 for unlimited).
 * max-scripts = 20         ; max number of scripts to run in concurrently (-1 for unlimited)
 * executable = true        ; runs this script if the executable bit is set.
 * dir-prefix = /cgi-bin/   ; directory prefix cgi scripts must reside inside (well known default: /cgi-bin/)
 *
 * [cgi.mappings]
 * .pl = /usr/bin/perl 	    ; maps all .pl to /usr/bin/perl
 * .php = /usr/bin/php      ; maps all .php files to /usr/bin/php
 * .rb = /usr/bin/ruby      ; maps all .ruby files to /usr/bin/ruby
 *
 * ttl/kill-ttl/max-scripts are not yet implemented!
 */

/* TODO
 *
 * - properly handle CGI script errors (early exits, no content, ...)
 * - allow keep-alive on fast closing childs by adding content-length manually (if not set already)
 * - pass child's stderr to some proper log stream destination
 * - close child's stdout when client connection dies away before child process terminated.
 *   - this implies, that we should still watch on the child process to terminate
 * - implement ttl handling
 * - implement max-scripts limit handling
 * - implement executable-only handling
 * - verify post-data passing
 *
 */

// {{{ cgi_response_parser
class cgi_response_parser :
	public boost::noncopyable
{
public:
	cgi_response_parser();

	boost::signal<void(const std::string&, const std::string&)> assign_header;
	boost::signal<void(const char *, const char *)> process_content;

	void process(const char *first, const char *last);

private:
	std::string name_;
	std::string value_;

	enum {
		parsing_header_name,
		parsing_header_value_ws_left,
		parsing_header_value,
		expecting_lf1,
		expecting_cr2,
		expecting_lf2,
		processing_content
	} state_;
};

inline cgi_response_parser::cgi_response_parser() :
	assign_header(),
	process_content(),
	name_(),
	value_(),
	state_(parsing_header_name)
{
}

inline void cgi_response_parser::process(const char *first, const char *last)
{
	//printf("Processing %ld bytes (state: %d)\n", last - first, state_);
	//::write(STDERR_FILENO, first, last - first);
	//printf("\nSTART PROCESSING:\n");

	if (state_ == processing_content)
	{
		process_content(first, last);
	}
	else
	{
		while (first != last)
		{
			switch (state_)
			{
				case parsing_header_name:
					if (*first == ':')
						state_ = parsing_header_value_ws_left;
					else if (*first == '\n') {
						state_ = processing_content;
						process_content(first - name_.size(), last);
					} else
						name_ += *first;
					++first;
					break;
				case parsing_header_value_ws_left:
					if (!std::isspace(*first)) {
						state_ = parsing_header_value;
						value_.clear();
						value_ += *first;
					}
					++first;
					break;
				case parsing_header_value:
					if (*first == '\r') {
						state_ = expecting_lf1;
					} else if (*first == '\n') {
						assign_header(name_, value_);
						state_ = expecting_cr2;
					} else {
						value_ += *first;
					}
					++first;
					break;
				case expecting_lf1:
					if (*first == '\n') {
						assign_header(name_, value_);
						state_ = expecting_cr2;
					} else {
						value_ += *first;
					}
					++first;
					break;
				case expecting_cr2:
					if (*first == '\r')
						state_ = expecting_lf2;
					else if (*first == '\n')
						state_ = processing_content;
					else {
						state_ = parsing_header_name;
						name_.clear();
						name_ += *first;
					}
					++first;
					break;
				case expecting_lf2:
					if (*first == '\n')					// [CR] LF [CR] LF
						state_ = processing_content;
					else {								// [CR] LF [CR] any
						state_ = parsing_header_name;
						name_.clear();
						name_ += *first;
					}
					++first;
					break;
				case processing_content:
					process_content(first, last);
					first = last;
					break;
			}
		}
	}
}
// }}}

// {{{ cgi_script
/** manages a CGI process.
 *
 * \code
 *	void handler(request& in, response& out)
 *	{
 *      cgi_script cgi(in, out, "/usr/bin/perl");
 *      cgi.ttl(boost::posix_time::seconds(60));        // define maximum ttl this script may run
 * 		cgi.async_run();
 * 	}
 * \endcode
 */
class cgi_script :
	public boost::enable_shared_from_this<cgi_script>,
	public boost::noncopyable
{
public:
	cgi_script(x0::request& in, x0::response& out, const std::string& hostprogram = "");
	~cgi_script();

	template<class CompletionHandler>
	void async_run(const CompletionHandler& handler);

	void async_run();

	static void async_run(x0::request& in, x0::response& out, const std::string& hostprogram = "");

private:
	/** feeds the HTTP request into the CGI's stdin pipe. */
	void transmitted_request(const asio::error_code& ec, std::size_t bytes_transferred);

	/** consumes the CGI's HTTP response header and body, validates and possibly modifies it, and then passes it to the actual client. */
	void receive_response(const asio::error_code& ec, std::size_t bytes_transferred);

	/** consumes any output read from the CGI's stderr pipe and either logs it into the web server's error log stream or passes it to the actual client stream, too. */
	void receive_error(const asio::error_code& ec, std::size_t bytes_transferred);

	void assign_header(const std::string& name, const std::string& value);
	void process_content(const char *first, const char *last);

private:
	x0::request& request_;
	x0::response& response_;
	std::string hostprogram_;

	x0::process process_;
	boost::array<char, 4096> outbuf_;
	boost::array<char, 4096> errbuf_;

	cgi_response_parser response_parser_;
	unsigned long long serial_;				//!< used to detect wether the cgi process actually generated a response or not.

	asio::deadline_timer ttl_;
};

cgi_script::cgi_script(x0::request& in, x0::response& out, const std::string& hostprogram)
  : request_(in), response_(out), hostprogram_(hostprogram),
	process_(in.connection.server().io_service()),
	outbuf_(), errbuf_(),
	response_parser_(),
	serial_(0),
	ttl_(in.connection.server().io_service())
{
	//printf("cgi_script(entity=\"%s\", hostprogram=\"%s\")\n", request_.entity.c_str(), hostprogram_.c_str());

	response_parser_.assign_header.connect(boost::bind(&cgi_script::assign_header, this, _1, _2));
	response_parser_.process_content.connect(boost::bind(&cgi_script::process_content, this, _1, _2));
}

cgi_script::~cgi_script()
{
	//printf("~cgi_script(entity=\"%s\", hostprogram=\"%s\")\n", request_.entity.c_str(), hostprogram_.c_str());
}

void cgi_script::async_run(x0::request& in, x0::response& out, const std::string& hostprogram)
{
	if (cgi_script *cgi = new cgi_script(in, out, hostprogram))
	{
		cgi->async_run();
	}
}

static inline void _loadenv_if(const std::string& name, x0::process::environment& environment)
{
	if (const char *value = ::getenv(name.c_str()))
	{
		environment[name] = value;
	}
}

inline void cgi_script::async_run()
{
	std::string workdir(request_.document_root);

	x0::process::params params;
	params.push_back(request_.fileinfo->filename());

	// {{{ setup request / initialize environment and handler
	x0::process::environment environment;

	environment["SERVER_SOFTWARE"] = PACKAGE_NAME "/" PACKAGE_VERSION;
	environment["SERVER_NAME"] = request_.header("Host");
	environment["GATEWAY_INTERFACE"] = "CGI/1.1";

	environment["SERVER_PROTOCOL"] = "1.1"; // XXX or 1.0
	environment["SERVER_ADDR"] = "localhost";
	environment["SERVER_PORT"] = "8080";

	environment["REQUEST_METHOD"] = request_.method;
	environment["PATH_INFO"] = request_.path;
	environment["PATH_TRANSLATED"] = request_.fileinfo->filename();
	environment["SCRIPT_NAME"] = request_.path;
	environment["QUERY_STRING"] = request_.query;			// unparsed uri
	environment["REQUEST_URI"] = request_.uri;

	//environment["REMOTE_HOST"] = "";  // optional
	environment["REMOTE_ADDR"] = request_.connection.client_ip();
	environment["REMOTE_PORT"] = boost::lexical_cast<std::string>(request_.connection.client_port());

	//environment["AUTH_TYPE"] = "";
	//environment["REMOTE_USER"] = "";
	//environment["REMOTE_IDENT"] = "";

	if (request_.body.empty())
	{
		environment["CONTENT_LENGTH"] = "0";
		process_.input().close();
	}
	else
	{
		environment["CONTENT_TYPE"] = request_.header("Content-Type");
		environment["CONTENT_LENGTH"] = request_.header("Content-Length");

		asio::async_write(process_.input(), asio::buffer(request_.body),
			boost::bind(&cgi_script::transmitted_request, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
	}

#if X0_SSL
	if (request_.connection.secure())
	{
		environment["HTTPS"] = "1";
	}
#endif

	environment["SCRIPT_FILENAME"] = request_.fileinfo->filename();
	environment["DOCUMENT_ROOT"] = request_.document_root;

	// HTTP request headers
	for (auto i = request_.headers.begin(), e = request_.headers.end(); i != e; ++i)
	{
		std::string key;
		key.reserve(5 + i->name.size());
		key += "HTTP_";

		for (const char *p = i->name.c_str(); *p; ++p)
		{
			key += std::isalnum(*p) ? std::toupper(*p) : '_';
		}

		std::string value(i->value);

		environment[key] = value;
	}

	// platfrom specifics
#ifdef __CYGWIN__
	_loadenv_if("SYSTEMROOT", environment);
#endif

	// for valgrind
	_loadenv_if("LD_PRELOAD", environment);
	_loadenv_if("LD_LIBRARY_PATH", environment);
	// }}}

	// redirect process_'s stdout/stderr to own member functions to handle its response
	asio::async_read(process_.output(), asio::buffer(outbuf_),
		boost::bind(&cgi_script::receive_response, this, asio::placeholders::error, asio::placeholders::bytes_transferred));

	asio::async_read(process_.error(), asio::buffer(errbuf_),
		boost::bind(&cgi_script::receive_error, this, asio::placeholders::error, asio::placeholders::bytes_transferred));

	// actually start child process
	process_.start(hostprogram_, params, environment, workdir);
}

void cgi_script::transmitted_request(const asio::error_code& ec, std::size_t bytes_transferred)
{
	//printf("cgi_script::transmitted_request(%s, %ld/%ld)\n", ec.message().c_str(), bytes_transferred, request_.body.size());
	process_.input().close();
}

void cgi_script::receive_response(const asio::error_code& ec, std::size_t bytes_transferred)
{
	//printf("cgi_script::receive_response(%s, %ld)\n", ec.message().c_str(), bytes_transferred);

	if (bytes_transferred)
	{
		response_parser_.process(outbuf_.data(), outbuf_.data() + bytes_transferred);
		serial_++;
	}

	if (!ec)
	{
		asio::async_read(process_.output(), asio::buffer(outbuf_),
			boost::bind(&cgi_script::receive_response, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
	else if (ec != asio::error::operation_aborted)
	{
		if (!serial_)
		{
			response_.status = x0::response::internal_server_error;
			request_.connection.server().log(x0::severity::error, "CGI script generated no response: %s", request_.fileinfo->filename().c_str());
		}
		else if (!response_.has_header("Content-Length") && !response_.serializing())
		{
			// post-inject content-length in order to preserve keep-alive in case we didn't start serializing yet and
			// the client requested persistent connections.
			response_ += x0::header("Content-Length", boost::lexical_cast<std::string>(response_.content_length()));
		}

		response_.flush();
		delete this; //destroy();
	}
}

void cgi_script::receive_error(const asio::error_code& ec, std::size_t bytes_transferred)
{
	//printf("cgi_script::receive_error(%s, %ld)\n", ec.message().c_str(), bytes_transferred);

	if (bytes_transferred)
	{
		// maybe i should cache it and then log it line(s)-wise
		std::string msg(outbuf_.data(), outbuf_.data() + bytes_transferred);
		request_.connection.server().log(x0::severity::error, "CGI script error: %s", msg.c_str());
	}

	if (!ec)
	{
		asio::async_read(process_.error(), asio::buffer(errbuf_),
			boost::bind(&cgi_script::receive_error, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
}

void cgi_script::assign_header(const std::string& name, const std::string& value)
{
	//printf("assign_header(\"%s\", \"%s\")\n", name.c_str(), value.c_str());
	response_.header(name, value);
}

void cgi_script::process_content(const char *first, const char *last)
{
	//printf("process_content(length=%ld)\n", last - first);
	response_.write(std::string(first, last));
}
// }}}

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class cgi_plugin :
	public x0::plugin
{
public:
	cgi_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		interpreter_(),
		process_executables_(),
		prefix_(),
		ttl_()
	{
		c = server_.generate_content.connect(boost::bind(&cgi_plugin::generate_content, this, _1, _2));
	}

	~cgi_plugin() {
		server_.generate_content.disconnect(c);
	}

	virtual void configure()
	{
		// reset to defaults
		prefix_ = "/cgi-bin/";
		process_executables_ = false;
		interpreter_.clear();

		// load system config
		server_.config().load("CGI.PathPrefix", prefix_);
		server_.config().load("CGI.Executable", process_executables_);
		server_.config().load("CGI.Mappings", interpreter_);
	}

private:
	/** content generator handler for this CGI plugin.
	 *
	 * \param in the client request
	 * \param out the outgoing server's response
	 * \retval true this plugin handled this request. no further content generator handler needs to be traversed.
	 * \retval false this plugin did not handle this request, it is adviced to traverse remaining content generator handlers.
	 *
	 * It first determines wether this request maps to an entity avilable as a regular file 
	 * on the local storage filesystem.
	 *
	 * If an interpreter-mapping can be found, this request is directly passed to that interpreter,
	 * otherwise, this entity is only to be executed if and only if the file is an executable
	 * and either processing executables is globally allowed or request path is part
	 * of the cgi prefix (usually /cgi-bin/).
	 */
	bool generate_content(x0::request& in, x0::response& out) {
		std::string path(in.fileinfo->filename());

		x0::fileinfo_ptr fi = in.connection.server().fileinfo(path);
		if (!fi)
			return false;

		if (!fi->is_regular())
			return false;

		std::string interpreter;
		if (find_interpreter(in, interpreter))
		{
			cgi_script::async_run(in, out, interpreter);
			return true;
		}

		bool executable = fi->is_executable();

		if (executable && (process_executables_ || matches_prefix(in)))
		{
			cgi_script::async_run(in, out);
			return true;
		}

		return false;
	}

private:
	/** signal connection holder for this plugin's content generator. */
	x0::handler::connection c;

	/** a set of extension-to-interpreter mappings. */
	std::map<std::string, std::string> interpreter_;

	/** true, if allowed to run any entity marked as executable (x-bit set). */
	bool process_executables_;

	/** usually /cgi-bin/, a prefix inwhich everything is expected to be a cgi script. */
	std::string prefix_;

	/** time-to-live in seconds a CGI script may run at most. */
	int ttl_;

private:
	/** searches for an interpreter for this request.
	 *
	 * \param in the incoming request we search an interpreter executable for.
	 * \param interpreter out value used to store the result, if found.
	 * \retval true we found an interpreter for given request, its path is stored in \p interpreter.
	 * \retval false no interpreter found for given request.
	 *
	 * For wether or not an interpreter can be found is determined by the entities file extension
	 * this request maps to. If this extension is known/mapped to any interpreter in the local database,
	 * this value is used.
	 */
	bool find_interpreter(x0::request& in, std::string& interpreter)
	{
		std::string::size_type rpos = in.fileinfo->filename().rfind('.');

		if (rpos != std::string::npos)
		{
			std::string ext(in.fileinfo->filename().substr(rpos));
			auto i = interpreter_.find(ext);

			if (i != interpreter_.end())
			{
				interpreter = i->second;
				return true;
			}
		}
		return false;
	}

	/** tests wether the request's path matches the cgi-bin prefix.
	 * \retval true yes, it matches.
	 * \retval false no, it doesn't match.
	 */
	bool matches_prefix(x0::request& in)
	{
		if (std::strncmp(in.path.c_str(), prefix_.c_str(), prefix_.size()) == 0)
			return true;

		return false;
	}
};

X0_EXPORT_PLUGIN(cgi);
