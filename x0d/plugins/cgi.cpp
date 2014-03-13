/* <cgi.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: content generator
 *
 * description:
 *     Serves CGI/1.1 scripts
 *
 * setup API:
 *     int cgi.ttl = 5;                    ; max time in seconds a cgi may run until SIGTERM is issued (-1 for unlimited).
 *     int cgi.kill_ttl = 5                ; max time to wait from SIGTERM on before a SIGKILL is ussued (-1 for unlimited).
 *     int cgi.max_scripts = 20            ; max number of scripts to run in concurrently (-1 for unlimited)
 *
 * request processing API:
 *     handler cgi.exec()                  ; processes executable files as CGI (apache-style: ExecCGI-option)
 *     handler cgi.run(string executable)  ; processes given executable as CGI on current requested file
 *
 * notes:
 *     ttl/kill-ttl/max-scripts are not yet implemented!
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpMessageParser.h>
#include <x0/io/BufferRefSource.h>
#include <x0/strutils.h>
#include <x0/Process.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <system_error>
#include <algorithm>
#include <string>
#include <cctype>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#if 0 // !defined(XZERO_NDEBUG)
#	define TRACE(msg...) DEBUG(msg)
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
 *	void handler(request& in)
 *	{
 *      CgiScript cgi(in, "/usr/bin/perl");
 *      cgi.ttl(TimeSpan::fromSeconds(60));     // define maximum ttl this script may run
 * 		cgi.runAsync();
 * 	}
 * \endcode
 */
class CgiScript :
	public x0::HttpMessageParser
{
public:
	CgiScript(x0::HttpRequest *in, const std::string& hostprogram = "");
	~CgiScript();

	void runAsync();

	static void runAsync(x0::HttpRequest *in, const std::string& hostprogram = "");

	void log(x0::LogMessage&& msg);

private:
	// CGI program's response message processor hooks
	virtual bool onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool onMessageContent(const x0::BufferRef& content);

	// CGI program's I/O callback handlers
	void onStdinReady(ev::io& w, int revents);
	void onStdinAvailable(const x0::BufferRef& chunk);
	void onStdoutAvailable(ev::io& w, int revents);
	void onStderrAvailable(ev::io& w, int revents);

	// client's I/O completion handlers
	void onStdoutWritten();
	static void onAbort(void *p);

	// child exit watcher
	void onChild(ev::child&, int revents);
	void onCheckDestroy(ev::async& w, int revents);
	bool checkDestroy();

private:
	enum OutputFlags
	{
		NoneClosed   = 0,

		StdoutClosed = 1,
		StderrClosed = 2,
		ChildClosed  = 4,

		OutputClosed    = StdoutClosed | StderrClosed | ChildClosed
	};

private:
	struct ev_loop *loop_;
	ev::child evChild_;		//!< watcher for child-exit event
	ev::async evCheckDestroy_;

	x0::HttpRequest *request_;
	std::string hostprogram_;

	x0::Process process_;
	x0::Buffer outbuf_;
	x0::Buffer errbuf_;

	unsigned long long serial_;				//!< used to detect wether the cgi process actually generated a response or not.

	ev::io evStdin_;						//!< cgi script's stdin watcher
	ev::io evStdout_;						//!< cgi script's stdout watcher
	ev::io evStderr_;						//!< cgi script's stderr watcher
	ev::timer ttl_;							//!< TTL watcher

	x0::Buffer stdinTransferBuffer_;
	enum { StdinFinished, StdinActive, StdinWaiting } stdinTransferMode_;
	size_t stdinTransferOffset_;			//!< current write-offset into the transfer buffer

	x0::Buffer stdoutTransferBuffer_;
	bool stdoutTransferActive_;

	unsigned outputFlags_;
};

CgiScript::CgiScript(x0::HttpRequest *in, const std::string& hostprogram) :
	HttpMessageParser(x0::HttpMessageParser::MESSAGE),
	loop_(in->connection.worker().loop()),
	evChild_(in->connection.worker().server().loop()),
	evCheckDestroy_(loop_),
	request_(in),
	hostprogram_(hostprogram),
	process_(loop_),
	outbuf_(), errbuf_(),
	serial_(0),
	evStdin_(loop_),
	evStdout_(loop_),
	evStderr_(loop_),
	ttl_(loop_),
	stdinTransferBuffer_(),
	stdinTransferMode_(StdinFinished),
	stdinTransferOffset_(0),
	stdoutTransferBuffer_(),
	stdoutTransferActive_(false),
	outputFlags_(NoneClosed)
{
	TRACE("CgiScript(path=\"%s\", hostprogram=\"%s\")", request_->fileinfo->path().c_str(), hostprogram_.c_str());

	evStdin_.set<CgiScript, &CgiScript::onStdinReady>(this);
	evStdout_.set<CgiScript, &CgiScript::onStdoutAvailable>(this);
	evStderr_.set<CgiScript, &CgiScript::onStderrAvailable>(this);

	request_->setAbortHandler(&CgiScript::onAbort, this);
}

CgiScript::~CgiScript()
{
	TRACE("destructing");
	if (request_) {
		if (request_->status == x0::HttpStatus::Undefined) {
			// we got killed before we could actually generate a response
			request_->status = x0::HttpStatus::ServiceUnavailable;
		}

		request_->setAbortHandler(nullptr);
		request_->finish();
	}
}

/** callback, invoked when child process status changed.
 *
 * \note This is potentially <b>NOT</b> from within the thread the CGI script is
 * being handled in, which is, because child process may be only watched
 * from within the default (main) event loop, which is an indirect restriction
 * by libev.
 */
void CgiScript::onChild(ev::child&, int revents)
{
	TRACE("onChild(0x%x)", revents);
	evCheckDestroy_.send();
}

void CgiScript::onCheckDestroy(ev::async& /*w*/, int /*revents*/)
{
	// libev invoked waitpid() for us, so re-use its results by passing it to Process
	// directly instead of letting it invoke waitpid() again, which might bail out
	// with ECHILD in case the process already exited, because its task_struct 
	// has been removed due to libev's waitpid() invokation already.
	process_.setStatus(evChild_.rstatus);

	if (process_.expired()) {
		// process exited; do not wait for any child I/O stream to complete, just kill us.
		outputFlags_ |= ChildClosed;
		checkDestroy();
	}
}

/** conditionally destructs this object.
 *
 * The object gets only destroyed if all conditions meet:
 * <ul>
 *   <li>process must be exited</li>
 *   <li>stdout pipe must be disconnected</li>
 * </ul>
 *
 * \retval true object destroyed.
 * \retval false object not destroyed.
 */
bool CgiScript::checkDestroy()
{
	// child's stdout still open?
	if ((outputFlags_ & OutputClosed) == OutputClosed)
	{
		TRACE("checkDestroy: all subjects closed (0x%04x)", outputFlags_);
		delete this;
		return true;
	}

	std::string fs;
	if (outputFlags_ & StdoutClosed)
		fs = "|stdout";
	if (outputFlags_ & StderrClosed)
		fs += "|stderr";
	if (outputFlags_ & ChildClosed)
		fs += "|child";
	fs += "|";

	TRACE("checkDestroy: failed (0x%04x) %s", outputFlags_, fs.c_str());
	return false;
}

void CgiScript::runAsync(x0::HttpRequest *in, const std::string& hostprogram)
{
	if (CgiScript *cgi = new CgiScript(in, hostprogram)) {
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
	std::string workdir(request_->documentRoot);
	x0::Process::ArgumentList params;
	std::string hostprogram;

	if (hostprogram_.empty()) {
		hostprogram = request_->fileinfo->path();
	} else {
		params.push_back(request_->fileinfo->path());
		hostprogram = hostprogram_;
	}

	// {{{ setup request / initialize environment and handler
	x0::Process::Environment environment;

	environment["SERVER_SOFTWARE"] = PACKAGE_NAME "/" PACKAGE_VERSION;
	environment["SERVER_NAME"] = request_->requestHeader("Host").str();
	environment["GATEWAY_INTERFACE"] = "CGI/1.1";

	environment["SERVER_PROTOCOL"] = "HTTP/1.1"; // XXX or 1.0
	environment["SERVER_ADDR"] = request_->connection.localIP().str();
	environment["SERVER_PORT"] = x0::lexical_cast<std::string>(request_->connection.localPort()); // TODO this should to be itoa'd only ONCE

	environment["REQUEST_METHOD"] = request_->method.str();
	environment["REDIRECT_STATUS"] = "200"; // for PHP configured with --force-redirect (Gentoo/Linux e.g.)

	environment["PATH_INFO"] = request_->pathinfo.str();
	if (!request_->pathinfo.empty()) {
		environment["PATH_TRANSLATED"] = request_->documentRoot + request_->pathinfo.str();
		environment["SCRIPT_NAME"] = request_->path.ref(0, request_->path.size() - request_->pathinfo.size()).str();
	} else {
		environment["SCRIPT_NAME"] = request_->path.str();
	}
	environment["QUERY_STRING"] = request_->query.str(); // unparsed uri
	environment["REQUEST_URI"] = request_->unparsedUri.str();

	//environment["REMOTE_HOST"] = "";  // optional
	environment["REMOTE_ADDR"] = request_->connection.remoteIP().str();
	environment["REMOTE_PORT"] = x0::lexical_cast<std::string>(request_->connection.remotePort());

	//environment["AUTH_TYPE"] = "";
	//environment["REMOTE_USER"] = "";
	//environment["REMOTE_IDENT"] = "";

	if (request_->contentAvailable()) {
		environment["CONTENT_TYPE"] = request_->requestHeader("Content-Type").str();
		environment["CONTENT_LENGTH"] = request_->requestHeader("Content-Length").str();

		request_->setBodyCallback<CgiScript, &CgiScript::onStdinAvailable>(this);
	} else {
		process_.closeInput();
	}

	if (request_->connection.isSecure()) {
		environment["HTTPS"] = "1";
	}

	environment["SCRIPT_FILENAME"] = request_->fileinfo->path();
	environment["DOCUMENT_ROOT"] = request_->documentRoot;

	// HTTP request headers
	for (auto i = request_->requestHeaders.begin(), e = request_->requestHeaders.end(); i != e; ++i) {
		x0::Buffer key;
		key.push_back("HTTP_");

		for (auto ch: i->name)
			key.push_back(std::isalnum(ch) ? std::toupper(ch) : '_');

		environment[key.c_str()] = i->value.str();
	}

	// platfrom specifics
#ifdef __CYGWIN__
	_loadenv_if("SYSTEMROOT", environment);
#endif

	// {{{ for valgrind
#ifndef XZERO_NDEBUG
	//_loadenv_if("LD_PRELOAD", environment);
	//_loadenv_if("LD_LIBRARY_PATH", environment);
#endif
	// }}}
	// }}}

#ifndef XZERO_NDEBUG
	for (auto i = environment.begin(), e = environment.end(); i != e; ++i)
		TRACE("env[%s]: '%s'", i->first.c_str(), i->second.c_str());
#endif

	// redirect process_'s stdout/stderr to own member functions to handle its response
	evStdout_.start(process_.output(), ev::READ);
	evStderr_.start(process_.output(), ev::READ);

	// actually start child process
	process_.start(hostprogram, params, environment, workdir);

	evChild_.set<CgiScript, &CgiScript::onChild>(this);
	evChild_.set(process_.id(), false);
	evChild_.start();

	evCheckDestroy_.set<CgiScript, &CgiScript::onCheckDestroy>(this);
	evCheckDestroy_.start();
#if !defined(NO_BUGGY_EVXX)
	// libev's ev++ (at least till version 3.80) does not initialize `sent` to zero)
	ev_async_set(&evCheckDestroy_);
#endif
}

/** writes request body chunk into stdin.
 * ready to read from request body (already available as \p chunk).
 */
void CgiScript::onStdinAvailable(const x0::BufferRef& chunk)
{
	TRACE("CgiScript.onStdinAvailable(chunksize=%ld)", chunk.size());

	// append chunk to transfer buffer
	stdinTransferBuffer_.push_back(chunk);

	// watch for stdin readiness to start/resume transfer
	if (stdinTransferMode_ != StdinActive)
	{
		evStdin_.start(process_.input(), ev::WRITE);
		stdinTransferMode_ = StdinActive;
	}
}

/** callback invoked when childs stdin is ready to receive.
 * ready to write into stdin.
 */
void CgiScript::onStdinReady(ev::io& /*w*/, int revents)
{
	TRACE("CgiScript::onStdinReady(%d)", revents);

	if (stdinTransferBuffer_.size() != 0)
	{
		ssize_t rv = ::write(process_.input(),
				stdinTransferBuffer_.data() + stdinTransferOffset_,
				stdinTransferBuffer_.size() - stdinTransferOffset_);

		if (rv < 0)
		{
			// error
			TRACE("- stdin write error: %s", strerror(errno));
		}
		else if (rv == 0)
		{
			// stdin closed by cgi process
			TRACE("- stdin closed by cgi proc");
		}
		else
		{
			TRACE("- wrote %ld/%ld bytes", rv, stdinTransferBuffer_.size() - stdinTransferOffset_);
			stdinTransferOffset_ += rv;

			if (stdinTransferOffset_ == stdinTransferBuffer_.size())
			{
				// buffer fully flushed
				stdinTransferOffset_ = 0;
				stdinTransferBuffer_.clear();
				evStdin_.stop();

				if (request_->contentAvailable()) {
					TRACE("-- buffer fully flushed. waiting for more from client");
					stdinTransferMode_ = StdinWaiting;
				} else {
					TRACE("-- buffer fully flushed. closing stdin.");
					stdinTransferMode_ = StdinFinished;
					process_.closeInput();
				}
			}
			else
			{
				// partial write, continue soon
				TRACE("-- continue write on data");
			}
		}
	}
	else // no more data to transfer
	{
		stdinTransferMode_ = StdinFinished;
		evStdin_.stop();
		process_.closeInput();
	}
}

/** consumes the CGI's HTTP response and passes it to the client.
 *
 *  includes validation and possible post-modification
 */
void CgiScript::onStdoutAvailable(ev::io& w, int revents)
{
	TRACE("onStdoutAvailable()");

	if (!request_) {
		// no client request (anymore)
		TRACE("no client request (anymore)");
		evStdout_.stop();
		outputFlags_ |= StdoutClosed;
		return;
	}

	std::size_t lower_bound = outbuf_.size();

	if (lower_bound == outbuf_.capacity())
		outbuf_.setCapacity(outbuf_.capacity() + 4096);

	int rv = ::read(process_.output(), outbuf_.end(), outbuf_.capacity() - lower_bound);

	if (rv > 0)
	{
		TRACE("onStdoutAvailable(): read %d bytes", rv);

		outbuf_.resize(lower_bound + rv);
		//printf("%s\n", outbuf_.ref(outbuf_.size() - rv, rv).str().c_str());

		std::size_t np = parseFragment(outbuf_.ref(lower_bound, rv));
		(void) np;

		TRACE("onStdoutAvailable@process: %ld", np);

		serial_++;
	}
	else if (rv < 0)
	{
		TRACE("onStdoutAvailable: rv=%d %s", rv, strerror(errno));
		if (rv != EINTR && rv != EAGAIN)
		{
			// error while reading from stdout
			evStdout_.stop();
			outputFlags_ |= StdoutClosed;

			request_->log(x0::Severity::error,
				"CGI: error while reading on stdout of: %s: %s",
				request_->fileinfo->path().c_str(),
				strerror(errno));

			if (!serial_)
			{
				request_->status = x0::HttpStatus::InternalServerError;
				request_->log(x0::Severity::error, "CGI script generated no response: %s", request_->fileinfo->path().c_str());
			}
		}
	}
	else // if (rv == 0)
	{
		// stdout closed by cgi child process
		TRACE("stdout closed");

		evStdout_.stop();
		outputFlags_ |= StdoutClosed;

		checkDestroy();
	}
}

/** consumes any output read from the CGI's stderr pipe and either logs it into the web server's error log stream or passes it to the actual client stream, too. */
void CgiScript::onStderrAvailable(ev::io& /*w*/, int revents)
{
	TRACE("onStderrAvailable()");
	if (!request_) {
		TRACE("no client request (anymore)");
		evStderr_.stop();
		outputFlags_ |= StderrClosed;
		return;
	}

	int rv = ::read(process_.error(), (char *)errbuf_.data(), errbuf_.capacity());

	if (rv > 0)
	{
		TRACE("read %d bytes: %s", rv, errbuf_.data());
		errbuf_.resize(rv);
		request_->log(x0::Severity::error,
			"CGI script error: %s: %s",
			request_->fileinfo->path().c_str(),
			errbuf_.str().c_str());
	}
	else if (rv == 0)
	{
		TRACE("stderr closed");
		evStderr_.stop();
		outputFlags_ |= StderrClosed;
		checkDestroy();
	}
	else // if (rv < 0)
	{
		if (errno != EINTR && errno != EAGAIN)
		{
			request_->log(x0::Severity::error,
				"CGI: error while reading on stderr of: %s: %s",
				request_->fileinfo->path().c_str(),
				strerror(errno));

			evStderr_.stop();
			outputFlags_ |= StderrClosed;
		}
	}
}

void CgiScript::log(x0::LogMessage&& msg)
{
	if (request_) {
		msg.addTag("cgi");
		request_->log(std::move(msg));
	}
}

bool CgiScript::onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value)
{
	TRACE("messageHeader(\"%s\", \"%s\")", name.str().c_str(), value.str().c_str());

	if (name == "Status") {
		int status = value.ref(0, value.find(' ')).toInt();
		request_->status = static_cast<x0::HttpStatus>(status);
	} else {
		if (name == "Location")
			request_->status = x0::HttpStatus::MovedTemporarily;

		request_->responseHeaders.push_back(name.str(), value.str());
	}

	return true;
}

bool CgiScript::onMessageContent(const x0::BufferRef& value)
{
	TRACE("messageContent(length=%ld)", value.size());

	if (stdoutTransferActive_) {
		stdoutTransferBuffer_.push_back(value);
	} else {
		stdoutTransferActive_ = true;
		evStdout_.stop();
		request_->write<x0::BufferRefSource>(value);
		request_->writeCallback<CgiScript, &CgiScript::onStdoutWritten>(this);
	}

	return false;
}

/** completion handler for the response content stream.
 */
void CgiScript::onStdoutWritten()
{
	TRACE("onStdoutWritten()");

	stdoutTransferActive_ = false;

	if (stdoutTransferBuffer_.size() > 0) {
		TRACE("flushing stdoutBuffer (%ld)", stdoutTransferBuffer_.size());
		request_->write<x0::BufferRefSource>(stdoutTransferBuffer_.ref());
		request_->writeCallback<CgiScript, &CgiScript::onStdoutWritten>(this);
	} else {
		TRACE("stdout: watch");
		evStdout_.start();
	}
}

void CgiScript::onAbort(void *p)
{
	CgiScript *self = (CgiScript *) p;

	TRACE("onAbort()");

	// SIGTERM will also implicitely cause the request to be finish()ed - immediately.
	self->process_.terminate();
}
// }}}

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class CgiPlugin :
	public x0d::XzeroPlugin
{
private:
	/** time-to-live in seconds a CGI script may run at most. */
	long long ttl_;

public:
    CgiPlugin(x0d::XzeroDaemon* d, const std::string& name) :
        x0d::XzeroPlugin(d, name),
        ttl_(0)
    {
        setupFunction("cgi.ttl", &CgiPlugin::set_ttl, x0::FlowType::Number);

        mainHandler("cgi.exec", &CgiPlugin::exec);
        mainHandler("cgi.run", &CgiPlugin::run, x0::FlowType::String);
    }

private:
	void set_ttl(x0::FlowVM::Params& args)
	{
        ttl_ = args.get<x0::FlowNumber>(1);
	}

	// handler cgi.exec();
	bool exec(x0::HttpRequest *in, x0::FlowVM::Params& args)
	{
		std::string path(in->fileinfo->path());

		auto fi = in->connection.worker().fileinfo(path);

		if (fi && fi->isRegular() && fi->isExecutable()) {
			CgiScript::runAsync(in);
			return true;
		}

		return false;
	}

    // handler cgi.run(string executable);
    bool run(x0::HttpRequest* r, x0::FlowVM::Params& args)
    {
        std::string interpreter = args.get<x0::FlowString*>(1)->str();

        CgiScript::runAsync(r, interpreter);
        return true;
    }
};

X0_EXPORT_PLUGIN_CLASS(CgiPlugin)
