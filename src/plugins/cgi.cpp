/* <cgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpMessageProcessor.h>
#include <x0/io/BufferSource.h>
#include <x0/strutils.h>
#include <x0/Process.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/signal.hpp>

#include <system_error>
#include <algorithm>
#include <string>
#include <cctype>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#if 0 // !defined(NDEBUG)
#	define TRACE(msg...) DEBUG("cgi: " msg)
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

// {{{ CgiScript
/** manages a CGI process.
 *
 * \code
 *	void handler(request& in, response& out)
 *	{
 *      CgiScript cgi(in, out, "/usr/bin/perl");
 *      cgi.ttl(boost::posix_time::seconds(60));        // define maximum ttl this script may run
 * 		cgi.runAsync();
 * 	}
 * \endcode
 */
class CgiScript :
	public x0::HttpMessageProcessor
{
public:
	CgiScript(const std::function<void()>& done, x0::HttpRequest *in, x0::HttpResponse *out, const std::string& hostprogram = "");
	~CgiScript();

	template<class CompletionHandler>
	void runAsync(const CompletionHandler& handler);

	void runAsync();

	static void runAsync(const std::function<void()>& done, x0::HttpRequest *in, x0::HttpResponse *out, const std::string& hostprogram = "");

private:
	// CGI program's response message processor hooks
	virtual void messageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool messageContent(const x0::BufferRef& content);

	// CGI program's I/O callback handlers
	void onTransmitRequestBody(ev::io& w, int revents);
	void onResponseReceived(ev::io& w, int revents);
	void onErrorReceived(ev::io& w, int revents);

	// client's I/O completion handlers
	void onResponseTransmitted(int ec, std::size_t nb);

private:
	struct ev_loop *loop_;

	x0::HttpRequest *request_;
	x0::HttpResponse *response_;
	std::string hostprogram_;

	x0::Process process_;
	x0::Buffer outbuf_;
	x0::Buffer errbuf_;

	unsigned long long serial_;				//!< used to detect wether the cgi process actually generated a response or not.

	ev::io inwatch_;
	ev::io outwatch_;
	ev::io errwatch_;
	ev::timer ttl_;

	std::function<void()> done_;			//!< should call at least HttpResponse::finish()
	bool destroy_pending_;
};

CgiScript::CgiScript(const std::function<void()>& done, x0::HttpRequest *in, x0::HttpResponse *out, const std::string& hostprogram) :
	HttpMessageProcessor(x0::HttpMessageProcessor::MESSAGE),
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
	TRACE("CgiScript(path=\"%s\", hostprogram=\"%s\")\n", request_->path.str().c_str(), hostprogram_.c_str());
}

CgiScript::~CgiScript()
{
	TRACE("~CgiScript(path=\"%s\", hostprogram=\"%s\")\n", request_->path.str().c_str(), hostprogram_.c_str());
	done_();
}

void CgiScript::runAsync(const std::function<void()>& done, x0::HttpRequest *in, x0::HttpResponse *out, const std::string& hostprogram)
{
	if (CgiScript *cgi = new CgiScript(done, in, out, hostprogram))
	{
		cgi->runAsync();
	}
}

static inline void _loadenv_if(const std::string& name, x0::Process::Environment& environment)
{
	if (const char *value = ::getenv(name.c_str()))
	{
		environment[name] = value;
	}
}

inline void CgiScript::runAsync()
{
	std::string workdir(request_->document_root);
	x0::Process::ArgumentList params;
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
	x0::Process::Environment environment;

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

		inwatch_.set<CgiScript, &CgiScript::onTransmitRequestBody>(this);
		inwatch_.start(process_.input(), ev::WRITE);
	} */

#if defined(WITH_SSL)
	if (request_->connection.isSecure())
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

		for (auto p = i->name.begin(), q = i->name.end(); p != q; ++p)
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
	outwatch_.set<CgiScript, &CgiScript::onResponseReceived>(this);
	outwatch_.start(process_.output(), ev::READ);

	errwatch_.set<CgiScript, &CgiScript::onErrorReceived>(this);
	errwatch_.start(process_.output(), ev::READ);

	// actually start child process
	process_.start(hostprogram, params, environment, workdir);
}

/** feeds the HTTP request into the CGI's stdin pipe. */
void CgiScript::onTransmitRequestBody(ev::io& /*w*/, int revents)
{
	TRACE("CgiScript::transmitted_request(%s, %ld/%ld)\n", ec.message().c_str(), bytes_transferred, request_->body.size());
	//::close(process_.input());
}

/** consumes the CGI's HTTP response header and body, validates and possibly modifies it, and then passes it to the actual client. */
void CgiScript::onResponseReceived(ev::io& /*w*/, int revents)
{
	std::size_t lower_bound = outbuf_.size();

	if (lower_bound == outbuf_.capacity())
		outbuf_.capacity(outbuf_.capacity() + 4096);

	int rv = ::read(process_.output(), outbuf_.end(), outbuf_.capacity() - lower_bound);

	if (rv > 0)
	{
		TRACE("CgiScript.onResponseReceived(): read %d bytes\n", rv);

		outbuf_.resize(lower_bound + rv);

		std::size_t np = 0;
		std::error_code ec = process(outbuf_.ref(lower_bound, rv), np);
		TRACE("onResponseReceived@process: %s", ec.message().c_str());

		serial_++;
	}
	else if (rv == 0)
	{
		TRACE("CGI: closing stdout\n");
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
				request_->connection.server().log(x0::Severity::error, "CGI script generated no response: %s", request_->fileinfo->filename().c_str());
			}
			delete this;
		}
	}
}

/** consumes any output read from the CGI's stderr pipe and either logs it into the web server's error log stream or passes it to the actual client stream, too. */
void CgiScript::onErrorReceived(ev::io& /*w*/, int revents)
{
	TRACE("CgiScript::onErrorReceived()\n");

	int rv = ::read(process_.error(), (char *)errbuf_.data(), errbuf_.capacity());

	if (rv > 0)
	{
		TRACE("read %d bytes: %s\n", rv, errbuf_.data());
		errbuf_.resize(rv);
		request_->connection.server().log(x0::Severity::error, "CGI script error: %s", errbuf_.str().c_str());
	} else if (rv == 0) {
		TRACE("CGI: closing stderr\n");
		errwatch_.stop();
	} else {
		TRACE("onErrorReceived (rv=%d) %s\n", rv, strerror(errno));
	}
}

void CgiScript::messageHeader(const x0::BufferRef& name, const x0::BufferRef& value)
{
	TRACE("messageHeader(\"%s\", \"%s\")\n", name.str().c_str(), value.str().c_str());

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

bool CgiScript::messageContent(const x0::BufferRef& value)
{
	TRACE("messageContent(length=%ld) (%s)\n", value.size(), value.str().c_str());

	outwatch_.stop();

	response_->write(
		std::make_shared<x0::BufferSource>(value),
		std::bind(&CgiScript::onResponseTransmitted, this, std::placeholders::_1, std::placeholders::_2)
	);

	return false;
}

/** completion handler for the response content stream.
 */
void CgiScript::onResponseTransmitted(int ec, std::size_t nb)
{
	if (ec)
	{
		TRACE("onResponseTransmitted: client error: %s\n", strerror(errno));

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
	public x0::HttpPlugin,
	public x0::IHttpRequestHandler
{
private:
	/** usually /cgi-bin/, a prefix inwhich everything is expected to be a cgi script. */
	std::string prefix_;

	/** a set of extension-to-interpreter mappings. */
	std::map<std::string, std::string> interpreterMappings_;

	/** true, if allowed to run any entity marked as executable (x-bit set). */
	bool processExecutables_;

	/** time-to-live in seconds a CGI script may run at most. */
	int ttl_;

public:
	cgi_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		prefix_("/cgi-bin/"),
		interpreterMappings_(),
		processExecutables_(false),
		ttl_(0)
	{
		declareCVar("CgiPrefix", x0::HttpContext::server, &cgi_plugin::setPrefix);
		declareCVar("CgiMappings", x0::HttpContext::server, &cgi_plugin::setMappings);
		declareCVar("CgiExecutable", x0::HttpContext::server, &cgi_plugin::setExecutable);
	}

	std::error_code setPrefix(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(prefix_);
	}

	std::error_code setMappings(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(interpreterMappings_);
	}

	std::error_code setExecutable(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(processExecutables_);
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
	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args) {
		std::string path(in->fileinfo->filename());

		x0::FileInfoPtr fi = in->connection.server().fileinfo(path);
		if (!fi || fi->is_regular()) // not a (regular) file
			return false;

		std::string interpreter;
		if (lookupInterpreter(in, interpreter))
		{
			CgiScript::runAsync(std::bind(&x0::HttpResponse::finish, out), in, out, interpreter);
			return true;
		}

		if (fi->is_executable() && (processExecutables_ || matchesPrefix(in)))
		{
			CgiScript::runAsync(std::bind(&x0::HttpResponse::finish, out), in, out);
			return true;
		}

		// TODO: everything else, that matches the cgi prefix, should result into 403(?).

		return false;
	}

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
	bool lookupInterpreter(x0::HttpRequest *in, std::string& interpreter)
	{
		std::string::size_type rpos = in->fileinfo->filename().rfind('.');

		if (rpos != std::string::npos)
		{
			std::string ext(in->fileinfo->filename().substr(rpos));
			auto i = interpreterMappings_.find(ext);

			if (i != interpreterMappings_.end())
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
	bool matchesPrefix(x0::HttpRequest *in) const
	{
		if (in->path.begins(prefix_))
			return true;

		return false;
	}
};

X0_EXPORT_PLUGIN(cgi);
