/* <x0/mod_cgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/plugin.hpp>
#include <x0/http/server.hpp>
#include <x0/http/request.hpp>
#include <x0/http/response.hpp>
#include <x0/http/message_processor.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/strutils.hpp>
#include <x0/process.hpp>
#include <x0/types.hpp>
#include <x0/sysconfig.h>

#include <boost/system/error_code.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/signal.hpp>

#include <algorithm>
#include <string>
#include <cctype>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#if 0 // !defined(NDEBUG)
#	define TRACE(msg...) fprintf(stderr, msg)
#else
#	define TRACE(msg...) /*!*/
#endif

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
	public x0::message_processor
{
public:
	cgi_script(const std::function<void()>& done, x0::request *in, x0::response *out, const std::string& hostprogram = "");
	~cgi_script();

	template<class CompletionHandler>
	void async_run(const CompletionHandler& handler);

	void async_run();

	static void async_run(const std::function<void()>& done, x0::request *in, x0::response *out, const std::string& hostprogram = "");

private:
	/** feeds the HTTP request into the CGI's stdin pipe. */
	void transmit_request_body(ev::io& w, int revents);

	/** consumes the CGI's HTTP response header and body, validates and possibly modifies it, and then passes it to the actual client. */
	void receive_response(ev::io& w, int revents);

	/** consumes any output read from the CGI's stderr pipe and either logs it into the web server's error log stream or passes it to the actual client stream, too. */
	void receive_error(ev::io& w, int revents);

	virtual void message_header(const x0::buffer_ref& name, const x0::buffer_ref& value);
	virtual bool message_content(const x0::buffer_ref& content);

	void content_written(int ec, std::size_t nb);

private:
	struct ev_loop *loop_;

	x0::request *request_;
	x0::response *response_;
	std::string hostprogram_;

	x0::process process_;
	x0::buffer outbuf_;
	x0::buffer errbuf_;

	unsigned long long serial_;				//!< used to detect wether the cgi process actually generated a response or not.

	ev::io inwatch_;
	ev::io outwatch_;
	ev::io errwatch_;
	ev::timer ttl_;

	std::function<void()> done_;			//!< should call at least next.done()
	bool destroy_pending_;
};

cgi_script::cgi_script(const std::function<void()>& done, x0::request *in, x0::response *out, const std::string& hostprogram) :
	message_processor(x0::message_processor::MESSAGE),
	loop_(in->connection.server().loop()),
	request_(in),
	response_(out),
	hostprogram_(hostprogram),
	process_(loop_),
	outbuf_(), errbuf_(),
	serial_(0),
	inwatch_(loop_),
	outwatch_(loop_),
	errwatch_(loop_),
	ttl_(loop_),
	done_(done),
	destroy_pending_(false)
{
	TRACE("cgi_script(path=\"%s\", hostprogram=\"%s\")\n", request_->path.str().c_str(), hostprogram_.c_str());
}

cgi_script::~cgi_script()
{
	TRACE("~cgi_script(path=\"%s\", hostprogram=\"%s\")\n", request_->path.str().c_str(), hostprogram_.c_str());
	done_();
}

void cgi_script::async_run(const std::function<void()>& done, x0::request *in, x0::response *out, const std::string& hostprogram)
{
	if (cgi_script *cgi = new cgi_script(done, in, out, hostprogram))
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
	std::string workdir(request_->document_root);
	x0::process::params params;
	std::string hostprogram;

	if (hostprogram_.empty())
	{
		hostprogram = request_->fileinfo->filename();
	}
	else
	{
		params.push_back(request_->fileinfo->filename());
		hostprogram = hostprogram_;
	}

	// {{{ setup request / initialize environment and handler
	x0::process::environment environment;

	environment["SERVER_SOFTWARE"] = PACKAGE_NAME "/" PACKAGE_VERSION;
	environment["SERVER_NAME"] = request_->header("Host");
	environment["GATEWAY_INTERFACE"] = "CGI/1.1";

	environment["SERVER_PROTOCOL"] = "1.1"; // XXX or 1.0
	environment["SERVER_ADDR"] = "localhost";
	environment["SERVER_PORT"] = "8080";

	environment["REQUEST_METHOD"] = request_->method;
	environment["PATH_INFO"] = request_->path;
	environment["PATH_TRANSLATED"] = request_->fileinfo->filename();
	environment["SCRIPT_NAME"] = request_->path;
	environment["QUERY_STRING"] = request_->query;			// unparsed uri
	environment["REQUEST_URI"] = request_->uri;

	//environment["REMOTE_HOST"] = "";  // optional
	environment["REMOTE_ADDR"] = request_->connection.remote_ip();
	environment["REMOTE_PORT"] = boost::lexical_cast<std::string>(request_->connection.remote_port());

	//environment["AUTH_TYPE"] = "";
	//environment["REMOTE_USER"] = "";
	//environment["REMOTE_IDENT"] = "";

//	if (request_->body.empty())
	{
		environment["CONTENT_LENGTH"] = "0";
		::close(process_.input());
	}
/*	else
	{
		environment["CONTENT_TYPE"] = request_->header("Content-Type");
		environment["CONTENT_LENGTH"] = request_->header("Content-Length");

		inwatch_.set<cgi_script, &cgi_script::transmit_request_body>(this);
		inwatch_.start(process_.input(), ev::WRITE);
	} */

#if defined(WITH_SSL)
	if (request_->connection.ssl_enabled())
	{
		environment["HTTPS"] = "1";
	}
#endif

	environment["SCRIPT_FILENAME"] = request_->fileinfo->filename();
	environment["DOCUMENT_ROOT"] = request_->document_root;

	// HTTP request headers
	for (auto i = request_->headers.begin(), e = request_->headers.end(); i != e; ++i)
	{
		std::string key;
		key.reserve(5 + i->name.size());
		key += "HTTP_";

		for (x0::buffer::const_iterator p = i->name.begin(), q = i->name.end(); p != q; ++p)
		{
			key += std::isalnum(*p) ? std::toupper(*p) : '_';
		}

		environment[key] = i->value.str();
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
	outwatch_.set<cgi_script, &cgi_script::receive_response>(this);
	outwatch_.start(process_.output(), ev::READ);

	errwatch_.set<cgi_script, &cgi_script::receive_error>(this);
	errwatch_.start(process_.output(), ev::READ);

	// actually start child process
	process_.start(hostprogram, params, environment, workdir);
}

void cgi_script::transmit_request_body(ev::io& /*w*/, int revents)
{
	//TRACE("cgi_script::transmitted_request(%s, %ld/%ld)\n", ec.message().c_str(), bytes_transferred, request_->body.size());
	//::close(process_.input());
}

void cgi_script::receive_response(ev::io& /*w*/, int revents)
{
	std::size_t lower_bound = outbuf_.size();

	if (lower_bound == outbuf_.capacity())
		outbuf_.capacity(outbuf_.capacity() + 4096);

	int rv = ::read(process_.output(), outbuf_.end(), outbuf_.capacity() - lower_bound);

	if (rv > 0)
	{
		//TRACE("cgi_script.receive_response(): read %d bytes\n", rv);

		outbuf_.resize(lower_bound + rv);

		std::size_t np = 0;
		std::error_code ec = process(outbuf_.ref(lower_bound, rv), np);
		TRACE("receive_response@process: %s", ec.message().c_str());

		serial_++;
	}
	else if (rv == 0)
	{
		//TRACE("CGI: closing stdout\n");
		outwatch_.stop();
		delete this;
	}
	else // if (rv < 0)
	{
		if (rv != EINTR && rv != EAGAIN)
		{
			TRACE("CGI: stdout: %s\n", strerror(errno));
			outwatch_.stop();

			if (!serial_)
			{
				response_->status = x0::http_error::internal_server_error;
				request_->connection.server().log(x0::severity::error, "CGI script generated no response: %s", request_->fileinfo->filename().c_str());
			}
			delete this;
		}
	}
}

void cgi_script::receive_error(ev::io& /*w*/, int revents)
{
	//TRACE("cgi_script::receive_error()\n");

	int rv = ::read(process_.error(), (char *)errbuf_.data(), errbuf_.capacity());

	if (rv > 0)
	{
		//TRACE("read %d bytes: %s\n", rv, errbuf_.data());
		errbuf_.resize(rv);
		request_->connection.server().log(x0::severity::error, "CGI script error: %s", errbuf_.str().c_str());
	} else if (rv == 0) {
		//TRACE("CGI: closing stderr\n");
		errwatch_.stop();
	} else {
		TRACE("receive_error (rv=%d) %s\n", rv, strerror(errno));
	}
}

void cgi_script::message_header(const x0::buffer_ref& name, const x0::buffer_ref& value)
{
	//TRACE("message_header(\"%s\", \"%s\")\n", name.str().c_str(), value.str().c_str());

	if (name == "Status")
	{
		response_->status = static_cast<x0::http_error>(boost::lexical_cast<int>(value.str()));
	}
	else 
	{
		if (name == "Location")
			response_->status = x0::http_error::moved_temporarily;

		response_->headers.push_back(name.str(), value.str());
	}
}

bool cgi_script::message_content(const x0::buffer_ref& value)
{
	//TRACE("process_content(length=%ld) (%s)\n", value.size(), value.str().c_str());

	outwatch_.stop();

	response_->write(
		std::make_shared<x0::buffer_source>(value),
		std::bind(&cgi_script::content_written, this, std::placeholders::_1, std::placeholders::_2)
	);

	return false;
}

/** completion handler for the response content stream.
 */
void cgi_script::content_written(int ec, std::size_t nb)
{
	if (ec)
	{
		TRACE("content_written: client error: %s\n", strerror(errno));

		// kill cgi script as client disconnected.
		process_.terminate();
	}

	if (destroy_pending_)
	{
		TRACE("destroy pending!\n");
		delete this;
	}
	else
	{
		outwatch_.start();
	}
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
		c = server_.generate_content.connect(&cgi_plugin::generate_content, this);
		register_cvar("CGI.Mappings", x0::context::server, &cgi_plugin::configure_mappings);
	}

	~cgi_plugin() {
		c.disconnect();
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
	}

	bool configure_mappings(const x0::settings_value& cvar, x0::scope& s)
	{
		debug(0, "configure_mappings ...");
		if (!cvar.load(interpreter_))
			;//return false;

		debug(0, "configure_mappings: %s", tostring(interpreter_).c_str());
		return true;
	}

	static std::string tostring(const std::map<std::string, std::string>& map)
	{
		std::string rv;

		rv = "{";

		for (auto i = map.begin(), e = map.end(); i != e; ++i)
		{
			if (i != map.begin())
				rv += "; ";

			rv += "(";
			rv += i->first;
			rv += ",";
			rv += i->second;
			rv += ")";
		}
		rv += "}";

		return rv;
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
	void generate_content(x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out) {
		std::string path(in->fileinfo->filename());

		x0::fileinfo_ptr fi = in->connection.server().fileinfo(path);
		if (!fi)
			return next();

		if (!fi->is_regular())
			return next();

		std::string interpreter;
		if (find_interpreter(in, interpreter))
		{
			cgi_script::async_run(std::bind(&cgi_plugin::done, this, next), in, out, interpreter);
			return;
		}

		bool executable = fi->is_executable();

		if (executable && (process_executables_ || matches_prefix(in)))
		{
			cgi_script::async_run(std::bind(&cgi_plugin::done, this, next), in, out);
			return;
		}

		return next();
	}

	void done(x0::request_handler::invokation_iterator next)
	{
		next.done();
	}

private:
	/** signal connection holder for this plugin's content generator. */
	x0::request_handler::connection c;

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
	bool find_interpreter(x0::request *in, std::string& interpreter)
	{
		std::string::size_type rpos = in->fileinfo->filename().rfind('.');

		if (rpos != std::string::npos)
		{
			std::string ext(in->fileinfo->filename().substr(rpos));
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
	bool matches_prefix(x0::request *in)
	{
		if (in->path.begins(prefix_))
			return true;

		return false;
	}
};

X0_EXPORT_PLUGIN(cgi);
