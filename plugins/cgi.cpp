/* <cgi.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: content generator
 *
 * description:
 *     Serves CGI/1.1 scripts
 *
 * setup API:
 *     int cgi.ttl = 5;                ; max time in seconds a cgi may run until SIGTERM is issued (-1 for unlimited).
 *     int cgi.kill_ttl = 5            ; max time to wait from SIGTERM on before a SIGKILL is ussued (-1 for unlimited).
 *     int cgi.max_scripts = 20        ; max number of scripts to run in concurrently (-1 for unlimited)
 *     hash cgi.mapping = {}           ; list of file-extension/program pairs for running several cgi scripts with.
 *
 * request processing API:
 *     handler cgi.prefix(prefix => path) ; processes prefix-mapped executables as CGI (well known ScriptAlias)
 *     handler cgi.exec()                 ; processes executable files as CGI (apache-style: ExecCGI-option)
 *     handler cgi.map()                  ; processes cgi mappings
 *
 * notes:
 *     ttl/kill-ttl/max-scripts are not yet implemented!
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
	virtual void messageHeader(x0::BufferRef&& name, x0::BufferRef&& value);
	virtual bool messageContent(x0::BufferRef&& content);

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
	TRACE("CgiScript(path=\"%s\", hostprogram=\"%s\")", request_->fileinfo->filename().c_str(), hostprogram_.c_str());
}

CgiScript::~CgiScript()
{
	TRACE("~CgiScript(path=\"%s\", hostprogram=\"%s\")", request_->fileinfo->filename().c_str(), hostprogram_.c_str());
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
	environment["SERVER_NAME"] = request_->header("Host").str();
	environment["GATEWAY_INTERFACE"] = "CGI/1.1";

	environment["SERVER_PROTOCOL"] = "1.1"; // XXX or 1.0
	environment["SERVER_ADDR"] = request_->connection.localIP();
	environment["SERVER_PORT"] = boost::lexical_cast<std::string>(request_->connection.localPort()); // TODO this should to be itoa'd only ONCE

	environment["REQUEST_METHOD"] = request_->method.str();

	environment["PATH_INFO"] = request_->pathinfo;
	if (!request_->pathinfo.empty())
		environment["PATH_TRANSLATED"] = request_->document_root + request_->pathinfo;

	environment["SCRIPT_NAME"] = request_->path.str();
	environment["QUERY_STRING"] = request_->query.str(); // unparsed uri
	environment["REQUEST_URI"] = request_->uri.str();

	//environment["REMOTE_HOST"] = "";  // optional
	environment["REMOTE_ADDR"] = request_->connection.remoteIP();
	environment["REMOTE_PORT"] = boost::lexical_cast<std::string>(request_->connection.remotePort());

	//environment["AUTH_TYPE"] = "";
	//environment["REMOTE_USER"] = "";
	//environment["REMOTE_IDENT"] = "";

//	if (request_->body.empty())
	{
		process_.closeInput();
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

	//for (auto i = environment.begin(), e = environment.end(); i != e; ++i)
	//	TRACE("env[%s]: '%s'", i->first.c_str(), i->second.c_str());

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
	TRACE("CgiScript::onTransmitRequestBody(%d)", revents);
	// process_.closeInput();
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
		TRACE("CgiScript.onResponseReceived(): read %d bytes", rv);

		outbuf_.resize(lower_bound + rv);
		//printf("%s\n", outbuf_.ref(outbuf_.size() - rv, rv).str().c_str());

		std::size_t np = 0;
		std::error_code ec = process(outbuf_.ref(lower_bound, rv), np);
		TRACE("onResponseReceived@process: %s; %ld", ec.message().c_str(), np);

		serial_++;
	}
	else if (rv == 0)
	{
		TRACE("CGI: stdout closed");
		outwatch_.stop();
		delete this;
	}
	else // if (rv < 0)
	{
		if (rv != EINTR && rv != EAGAIN)
		{
			TRACE("CGI: stdout: %s", strerror(errno));
			outwatch_.stop();

			if (!serial_)
			{
				response_->status = x0::HttpError::InternalServerError;
				request_->connection.server().log(x0::Severity::error, "CGI script generated no response: %s", request_->fileinfo->filename().c_str());
			}
			delete this;
		}
	}
}

/** consumes any output read from the CGI's stderr pipe and either logs it into the web server's error log stream or passes it to the actual client stream, too. */
void CgiScript::onErrorReceived(ev::io& /*w*/, int revents)
{
	TRACE("CgiScript::onErrorReceived()");

	int rv = ::read(process_.error(), (char *)errbuf_.data(), errbuf_.capacity());

	if (rv > 0)
	{
		TRACE("read %d bytes: %s", rv, errbuf_.data());
		errbuf_.resize(rv);
		request_->connection.server().log(x0::Severity::error, "CGI script error: %s", errbuf_.str().c_str());
	} else if (rv == 0) {
		TRACE("CGI: stderr closed");
		errwatch_.stop();
	} else {
		TRACE("onErrorReceived (rv=%d) %s", rv, strerror(errno));
	}
}

void CgiScript::messageHeader(x0::BufferRef&& name, x0::BufferRef&& value)
{
	TRACE("messageHeader(\"%s\", \"%s\")", name.str().c_str(), value.str().c_str());

	if (name == "Status")
	{
		response_->status = static_cast<x0::HttpError>(boost::lexical_cast<int>(value.str()));
	}
	else 
	{
		if (name == "Location")
			response_->status = x0::HttpError::MovedTemporarily;

		response_->headers.push_back(name.str(), value.str());
	}
}

bool CgiScript::messageContent(x0::BufferRef&& value)
{
	TRACE("messageContent(length=%ld) (%s)", value.size(), value.str().c_str());

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
		TRACE("onResponseTransmitted: client error: %s", strerror(errno));

		// kill cgi script as client disconnected.
		process_.terminate();
	}

	if (destroy_pending_)
	{
		TRACE("destroy pending!");
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
	public x0::HttpPlugin
{
private:
	/** a set of extension-to-interpreter mappings. */
	std::map<std::string, std::string> interpreterMappings_;

	/** time-to-live in seconds a CGI script may run at most. */
	int ttl_;

public:
	cgi_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		interpreterMappings_(),
		ttl_(0)
	{
		registerSetupProperty<cgi_plugin, &cgi_plugin::set_ttl>("cgi.ttl", Flow::Value::NUMBER);
		registerSetupFunction<cgi_plugin, &cgi_plugin::set_mapping>("cgi.mapping", Flow::Value::VOID);

		registerHandler<cgi_plugin, &cgi_plugin::prefix>("cgi.prefix");
		registerHandler<cgi_plugin, &cgi_plugin::exec>("cgi.exec");
		registerHandler<cgi_plugin, &cgi_plugin::map>("cgi.map");
	}

private:
	// {{{ setup functions
	void set_ttl(Flow::Value& result, const x0::Params& args)
	{
		if (args.count() == 1 && args[0].isNumber())
			ttl_ = args[0].toNumber();
	}

	// cgi.mapping(ext => bin, ext => bin, ...);
	void set_mapping(Flow::Value& result, const x0::Params& args)
	{
		for (size_t i = 0; i < args.count(); ++i)
			addMapping(args[i]);
	}

	void addMapping(const Flow::Value& mapping)
	{
		if (!mapping.isArray())
			return;

		std::vector<const Flow::Value *> items;
		for (const Flow::Value *item = mapping.toArray(); !item->isVoid(); ++item)
			items.push_back(item);

		if (items.size() != 2)
		{
			for (const Flow::Value *item = mapping.toArray(); !item->isVoid(); ++item)
				addMapping(*item);
		}
		else if (items[0]->isString() && items[1]->isString())
		{
			interpreterMappings_[items[0]->toString()] = items[1]->toString();
		}
	}
	// }}}

	// {{{ request handler
	// cgi.prefix(prefix => path)
	bool prefix(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		const char *prefix = args[0][0].toString();
		const char *path = args[0][1].toString();

		if (!in->path.begins(prefix))
			return false;

		// rule: "/cgi-bin/" => "/var/www/localhost/cgi-bin/"
		// appl: "/cgi-bin/special/test.cgi" => "/var/www/localhost/cgi-bin/" "special/test.cgi"
		x0::Buffer phys;
		phys.push_back(path);
		phys.push_back(in->path.ref(strlen(prefix)));

		x0::FileInfoPtr fi = in->connection.server().fileinfo(phys.c_str());
		if (fi && fi->is_regular() && fi->is_executable())
		{
			in->fileinfo = fi;
			CgiScript::runAsync(std::bind(&x0::HttpResponse::finish, out), in, out);
			return true;
		}
		return false;
	}

	// handler cgi.exec();
	bool exec(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		std::string path(in->fileinfo->filename());

		x0::FileInfoPtr fi = in->connection.server().fileinfo(path);

		if (fi && fi->is_regular() && fi->is_executable())
		{
			CgiScript::runAsync(std::bind(&x0::HttpResponse::finish, out), in, out);
			return true;
		}

		return true;
	}

	// handler cgi.map();
	bool map(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		std::string path(in->fileinfo->filename());
		debug(0, "cgi.map: '%s'", path.c_str());

		x0::FileInfoPtr fi = in->connection.server().fileinfo(path);
		if (!fi)
			return false;
		
		if (!fi->is_regular())
			return false;

		std::string interpreter;
		if (!lookupInterpreter(in, interpreter))
			return false;

		debug(0, "runAsync...");
		CgiScript::runAsync(std::bind(&x0::HttpResponse::finish, out), in, out, interpreter);
		return true;
	}
	// }}}

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
		debug(0, "lookupInterpreter: rpos:%d", rpos);

		if (rpos != std::string::npos)
		{
			std::string ext(in->fileinfo->filename().substr(rpos));
			debug(0, "lookupInterpreter: ext:%s", ext.c_str());
			auto i = interpreterMappings_.find(ext);

			if (i != interpreterMappings_.end())
			{
				interpreter = i->second;
				debug(0, "lookupInterpreter: i:%s", interpreter.c_str());
				return true;
			}
		}
		return false;
	}
};

X0_EXPORT_PLUGIN(cgi)
