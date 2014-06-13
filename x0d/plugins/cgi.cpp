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
 *     int cgi.ttl = 5;                    ; max time in seconds a cgi may run until SIGTERM is issued (0 for unlimited).
 *     int cgi.kill_ttl = 5                ; max time to wait from SIGTERM on before a SIGKILL is ussued (0 for unlimited).
 *     int cgi.max_scripts = 20            ; max number of scripts to run in concurrently (0 for unlimited)
 *
 * request processing API:
 *     handler cgi.exec()                  ; processes executable files as CGI (apache-style: ExecCGI-option)
 *     handler cgi.run(string executable)  ; processes given executable as CGI on current requested file
 *
 * notes:
 *     ttl/kill-ttl are not yet implemented!
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpMessageParser.h>
#include <x0/io/BufferRefSource.h>
#include <x0/io/Source.h>
#include <x0/io/FileSink.h>
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

using namespace x0;

/* TODO
 *
 * - properly handle CGI script errors (early exits, no content, ...)
 * - allow keep-alive on fast closing childs by adding content-length manually (if not set already)
 * - pass child's stderr to some proper log stream destination
 * - close child's stdout when client connection dies away before child process terminated.
 *   - this implies, that we should still watch on the child process to terminate
 * - implement ttl handling
 * - implement executable-only handling
 * - verify post-data passing
 *
 */

// {{{ CgiScript
/** manages a CGI process.
 *
 * \code
 *	void handler(HttpRequest* r)
 *	{
 *      CgiScript cgi(r, "/usr/bin/perl");
 *      cgi.ttl(TimeSpan::fromSeconds(60));     // define maximum ttl this script may run
 * 		cgi.runAsync();
 * 	}
 * \endcode
 */
class CgiScript :
    public HttpMessageParser
{
public:
    CgiScript(HttpRequest* r, const std::string& hostprogram = "");
    ~CgiScript();

    void runAsync();

    static void runAsync(HttpRequest *in, const std::string& hostprogram = "");

    static size_t count() { return count_.load(); }

    void log(LogMessage&& msg);

    template<typename... Args>
    void log(Severity severity, const char* fmt, Args... args) {
        LogMessage msg(severity, fmt, std::move(args)...);
        msg.addTag("cgi");
        log(std::move(msg));
    }

private:
    // CGI program's response message processor hooks
    bool onMessageHeader(const BufferRef& name, const BufferRef& value) override;
    bool onMessageContent(const BufferRef& content) override;

    // CGI program's I/O callback handlers
    void onStdinReady(ev::io& w, int revents);
    void onStdoutAvailable(ev::io& w, int revents);
    void onStderrAvailable(ev::io& w, int revents);

    // client's I/O completion handlers
    void onStdoutWritten();

    // child exit watcher
    void onChild(ev::child&, int revents);
    void onCheckDestroy(ev::async& w, int revents);
    bool checkDestroy();

private:
    enum OutputFlags {
        NoneClosed   = 0,

        StdoutClosed = 1,
        StderrClosed = 2,
        ChildClosed  = 4,

        OutputClosed    = StdoutClosed | StderrClosed | ChildClosed
    };

private:
    static std::atomic<size_t> count_;

    struct ev_loop* loop_;
    ev::child evChild_;                     //!< watcher for child-exit event
    ev::async evCheckDestroy_;

    HttpRequest* request_;
    std::string hostprogram_;

    Process process_;
    Buffer outbuf_;
    Buffer errbuf_;

    unsigned long long serial_;				//!< used to detect wether the cgi process actually generated a response or not.

    ev::io evStdin_;						//!< cgi script's stdin watcher
    ev::io evStdout_;						//!< cgi script's stdout watcher
    ev::io evStderr_;						//!< cgi script's stderr watcher
    ev::timer ttl_;							//!< TTL watcher

    std::unique_ptr<Source> stdinSource_;
    std::unique_ptr<FileSink> stdinSink_;

    Buffer stdoutTransferBuffer_;
    bool stdoutTransferActive_;

    unsigned outputFlags_;
};

std::atomic<size_t> CgiScript::count_(0);

CgiScript::CgiScript(HttpRequest* r, const std::string& hostprogram) :
    HttpMessageParser(HttpMessageParser::MESSAGE),
    loop_(r->connection.worker().loop()),
    evChild_(r->connection.worker().server().loop()),
    evCheckDestroy_(loop_),
    request_(r),
    hostprogram_(hostprogram),
    process_(loop_),
    outbuf_(), errbuf_(),
    serial_(0),
    evStdin_(loop_),
    evStdout_(loop_),
    evStderr_(loop_),
    ttl_(loop_),

    stdinSource_(),
    stdinSink_(),

    stdoutTransferBuffer_(),
    stdoutTransferActive_(false),
    outputFlags_(NoneClosed)
{
    log(Severity::debug, "CgiScript(path=\"%s\", hostprogram=\"%s\")", request_->fileinfo->path().c_str(), hostprogram_.c_str());

    count_++;

    evStdin_.set<CgiScript, &CgiScript::onStdinReady>(this);
    evStdout_.set<CgiScript, &CgiScript::onStdoutAvailable>(this);
    evStderr_.set<CgiScript, &CgiScript::onStderrAvailable>(this);

    request_->setAbortHandler([this]() {
        process_.terminate();
    });
}

CgiScript::~CgiScript()
{
    log(Severity::debug, "destructing");

    if (request_) {
        if (request_->status == HttpStatus::Undefined) {
            request_->log(Severity::error, "we got killed before we could actually generate a response");
            request_->status = HttpStatus::ServiceUnavailable;
        }

        request_->finish();
        request_ = nullptr;
    }

    count_--;
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
    log(Severity::debug, "onChild(0x%x)", revents);
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
        log(Severity::debug, "checkDestroy: all subjects closed (0x%04x)", outputFlags_);
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

    log(Severity::debug, "checkDestroy: failed (0x%04x) %s", outputFlags_, fs.c_str());
    return false;
}

void CgiScript::runAsync(HttpRequest* r, const std::string& hostprogram)
{
    if (CgiScript* cgi = new CgiScript(r, hostprogram)) {
        cgi->runAsync();
    }
}

inline void _loadenv_if(const std::string& name, Process::Environment& environment)
{
    if (const char *value = ::getenv(name.c_str())) {
        environment[name] = value;
    }
}

inline void CgiScript::runAsync()
{
    Process::ArgumentList params;
    std::string hostprogram;

    if (hostprogram_.empty()) {
        hostprogram = request_->fileinfo->path();
    } else {
        // WHAT IF fileinfo not set yet?
        params.push_back(request_->fileinfo->path());
        hostprogram = hostprogram_;
    }

    // {{{ setup request / initialize environment and handler
    Process::Environment environment;

    environment["SERVER_SOFTWARE"] = PACKAGE_NAME "/" PACKAGE_VERSION;
    environment["SERVER_NAME"] = request_->requestHeader("Host").str();
    environment["GATEWAY_INTERFACE"] = "CGI/1.1";

    environment["SERVER_PROTOCOL"] = "HTTP/1.1"; // XXX or 1.0
    environment["SERVER_ADDR"] = request_->connection.localIP().str();
    environment["SERVER_PORT"] = std::to_string(request_->connection.localPort()); // TODO this should to be itoa'd only ONCE

    environment["REQUEST_METHOD"] = request_->method.str();
    environment["REDIRECT_STATUS"] = "200"; // for PHP configured with --force-redirect (Gentoo/Linux e.g.)

    request_->updatePathInfo();
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
    environment["REMOTE_PORT"] = lexical_cast<std::string>(request_->connection.remotePort());

    //environment["AUTH_TYPE"] = "";
    //environment["REMOTE_USER"] = "";
    //environment["REMOTE_IDENT"] = "";

    if (request_->contentAvailable()) {
        environment["CONTENT_TYPE"] = request_->requestHeader("Content-Type").str();
        environment["CONTENT_LENGTH"] = request_->requestHeader("Content-Length").str();
    } else {
        process_.closeInput();
    }

    if (request_->connection.isSecure()) {
        environment["HTTPS"] = "1";
    }

    environment["SCRIPT_FILENAME"] = request_->fileinfo->path();
    environment["DOCUMENT_ROOT"] = request_->documentRoot;

    // HTTP request headers
    for (const auto& header: request_->requestHeaders) {
        Buffer key = "HTTP_";

        for (auto ch: header.name) {
            key.push_back(static_cast<char>(std::isalnum(ch) ? std::toupper(ch) : '_'));
        }

        environment[key.c_str()] = header.value.str();
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
        log(Severity::debug, "env[%s]: '%s'", i->first.c_str(), i->second.c_str());
#endif

    // prepare stdin
    if (request_->contentAvailable()) {
        log(Severity::debug, "prepare stdin");
        stdinSource_ = std::move(request_->takeBody());
        stdinSink_.reset(new FileSink(process_.input(), false));
        evStdin_.start(process_.input(), ev::WRITE);
    } else {
        log(Severity::debug, "close stdin");
        process_.closeInput();
    }

    // redirect process_'s stdout/stderr to own member functions to handle its response
    evStdout_.start(process_.output(), ev::READ);
    evStderr_.start(process_.error(), ev::READ);

    // actually start child process
    std::string workdir(request_->documentRoot.str());
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

/** callback invoked when childs stdin is ready to receive.
 * ready to write into stdin.
 */
void CgiScript::onStdinReady(ev::io& /*w*/, int revents)
{
    log(Severity::debug, "CgiScript::onStdinReady(%d)", revents);

    for (;;) {
        ssize_t rv = stdinSource_->sendto(*stdinSink_);
        if (rv > 0) {
            log(Severity::debug, "- wrote %zi bytes to upstream's stdin", rv);
            continue;
        } else if (rv == 0) {
            // no more data to transfer
            log(Severity::debug, "- stdin transfer finished");
            evStdin_.stop();
            process_.closeInput();
            return;
        } else if (rv < 0) {
            switch (errno) {
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
                case EWOULDBLOCK:
#endif
                case EAGAIN:
                case EINTR:
                    return;
                default:
                    request_->log(Severity::error, "Writing request body to CGI failed. %s", strerror(errno));
                    evStdin_.stop();
                    process_.closeInput();
                    return;
            }
        }
    }
}

/** consumes the CGI's HTTP response and passes it to the client.
 *
 *  includes validation and possible post-modification
 */
void CgiScript::onStdoutAvailable(ev::io& w, int revents)
{
    log(Severity::debug, "onStdoutAvailable()");

    if (!request_) {
        // no client request (anymore)
        log(Severity::debug, "no client request (anymore)");
        evStdout_.stop();
        outputFlags_ |= StdoutClosed;
        return;
    }

    std::size_t lower_bound = outbuf_.size();

    if (lower_bound == outbuf_.capacity())
        outbuf_.setCapacity(outbuf_.capacity() + 4096);

    int rv = ::read(process_.output(), outbuf_.end(), outbuf_.capacity() - lower_bound);

    if (rv > 0) {
        log(Severity::debug, "onStdoutAvailable(): read %d bytes", rv);

        outbuf_.resize(lower_bound + rv);
        //printf("%s\n", outbuf_.ref(outbuf_.size() - rv, rv).str().c_str());

        std::size_t np = parseFragment(outbuf_.ref(lower_bound, rv));
        (void) np;

        log(Severity::debug, "onStdoutAvailable@process: %ld", np);

        serial_++;
    } else if (rv < 0) {
        log(Severity::debug, "onStdoutAvailable: rv=%d %s", rv, strerror(errno));
        if (rv != EINTR && rv != EAGAIN) {
            // error while reading from stdout
            evStdout_.stop();
            outputFlags_ |= StdoutClosed;

            request_->log(Severity::error,
                "CGI: error while reading on stdout of: %s: %s",
                request_->fileinfo->path().c_str(),
                strerror(errno));

            if (!serial_) {
                request_->status = HttpStatus::InternalServerError;
                request_->log(Severity::error, "CGI script generated no response: %s", request_->fileinfo->path().c_str());
            }
        }
    } else { // if (rv == 0) {
        // stdout closed by cgi child process
        log(Severity::debug, "stdout closed");

        evStdout_.stop();
        outputFlags_ |= StdoutClosed;

        checkDestroy();
    }
}

/** consumes any output read from the CGI's stderr pipe and either logs it into the web server's error log stream or passes it to the actual client stream, too. */
void CgiScript::onStderrAvailable(ev::io& /*w*/, int revents)
{
    log(Severity::debug, "onStderrAvailable()");
    if (!request_) {
        log(Severity::debug, "no client request (anymore)");
        evStderr_.stop();
        outputFlags_ |= StderrClosed;
        return;
    }

    int rv = ::read(process_.error(), (char *)errbuf_.data(), errbuf_.capacity());

    if (rv > 0) {
        log(Severity::debug, "read %d bytes: %s", rv, errbuf_.data());
        errbuf_.resize(rv);
        request_->log(Severity::error,
            "CGI script error: %s: %s",
            request_->fileinfo->path().c_str(),
            errbuf_.str().c_str());
    } else if (rv == 0) {
        log(Severity::debug, "stderr closed");
        evStderr_.stop();
        outputFlags_ |= StderrClosed;
        checkDestroy();
    } else { // if (rv < 0) {
        if (errno != EINTR && errno != EAGAIN) {
            request_->log(Severity::error,
                "CGI: error while reading on stderr of: %s: %s",
                request_->fileinfo->path().c_str(),
                strerror(errno));

            evStderr_.stop();
            outputFlags_ |= StderrClosed;
        }
    }
}

void CgiScript::log(LogMessage&& msg)
{
    if (request_) {
        msg.addTag("cgi");
        request_->log(std::move(msg));
    }
}

bool CgiScript::onMessageHeader(const BufferRef& name, const BufferRef& value)
{
    log(Severity::debug, "messageHeader(\"%s\", \"%s\")", name.str().c_str(), value.str().c_str());

    if (name == "Status") {
        int status = value.ref(0, value.find(' ')).toInt();
        request_->status = static_cast<HttpStatus>(status);
    } else {
        if (name == "Location" && !request_->status)
            request_->status = HttpStatus::MovedTemporarily;

        request_->responseHeaders.push_back(name, value);
    }

    return true;
}

bool CgiScript::onMessageContent(const BufferRef& value)
{
    log(Severity::debug, "messageContent(length=%ld)", value.size());

    if (stdoutTransferActive_) {
        stdoutTransferBuffer_.push_back(value);
    } else {
        stdoutTransferActive_ = true;
        evStdout_.stop();
        request_->write<BufferRefSource>(value);
        request_->writeCallback<CgiScript, &CgiScript::onStdoutWritten>(this);
    }

    return false;
}

/** completion handler for the response content stream.
 */
void CgiScript::onStdoutWritten()
{
    log(Severity::debug, "onStdoutWritten()");

    stdoutTransferActive_ = false;

    if (stdoutTransferBuffer_.size() > 0) {
        log(Severity::debug, "flushing stdoutBuffer (%ld)", stdoutTransferBuffer_.size());
        request_->write<BufferRefSource>(stdoutTransferBuffer_.ref());
        request_->writeCallback<CgiScript, &CgiScript::onStdoutWritten>(this);
    } else {
        log(Severity::debug, "stdout: watch");
        evStdout_.start();
    }
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
    size_t maxScripts_;

public:
    CgiPlugin(x0d::XzeroDaemon* d, const std::string& name) :
        x0d::XzeroPlugin(d, name),
        ttl_(0),
        maxScripts_(128)
    {
        setupFunction("cgi.ttl", &CgiPlugin::set_ttl, FlowType::Number);
        setupFunction("cgi.max_scripts", &CgiPlugin::setMaxScripts, FlowType::Number);

        mainHandler("cgi.exec", &CgiPlugin::exec);
        mainHandler("cgi.run", &CgiPlugin::run, FlowType::String);

        // TODO: DAG walk to "verify" that docroot was set by known core flow functions (docroot)
    }

private:
    void set_ttl(FlowVM::Params& args)
    {
        ttl_ = args.getInt(1);
    }

    void setMaxScripts(FlowVM::Params& args)
    {
        maxScripts_ = args.getInt(1);
    }

    // handler cgi.exec();
    bool exec(HttpRequest* r, FlowVM::Params& args)
    {
        if (!verify(r)) {
            return true;
        }

        std::string path = r->fileinfo->path();

        auto fi = r->connection.worker().fileinfo(path);

        if (fi && fi->isRegular() && fi->isExecutable()) {
            CgiScript::runAsync(r);
            return true;
        }

        return false;
    }

    // handler cgi.run(string executable);
    bool run(HttpRequest* r, FlowVM::Params& args)
    {
        if (!verify(r)) {
            return true;
        }

        std::string interpreter = args.getString(1).str();

        CgiScript::runAsync(r, interpreter);
        return true;
    }

    bool verify(HttpRequest* r) {
        if (maxScripts_ && CgiScript::count() > maxScripts_) {
            r->status = HttpStatus::ServiceUnavailable;
            r->responseHeaders.push_back("Retry-After", std::to_string(60));
            r->finish();
            return false;
        }

        if (!r->fileinfo) {
            log(Severity::error, "No document root set.");
            r->status = HttpStatus::InternalServerError;
            r->finish();
            return false;
        }

        return true;
    }
};

X0_EXPORT_PLUGIN_CLASS(CgiPlugin)
