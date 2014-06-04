/* <XzeroDaemon.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroDaemon.h>
#include <x0d/XzeroEventHandler.h>
#include <x0d/XzeroPlugin.h>
#include <x0d/XzeroCore.h>

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/flow/vm/Runner.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/FlowCallVisitor.h>
#include <x0/flow/FlowParser.h>
#include <x0/flow/ASTPrinter.h>
#include <x0/flow/AST.h>
#include <x0/flow/IRGenerator.h>
#include <x0/flow/TargetCodeGenerator.h>
#include <x0/flow/ir/PassManager.h>
#include <x0/flow/transform/UnusedBlockPass.h>
#include <x0/flow/transform/EmptyBlockElimination.h>
#include <x0/flow/transform/InstructionElimination.h>
#include <x0/io/SyslogSink.h>
#include <x0/Tokenizer.h>
#include <x0/Logger.h>
#include <x0/DateTime.h>
#include <x0/strutils.h>
#include <x0/Severity.h>
#include <x0/DebugLogger.h>
#include <x0/sysconfig.h>

#include <ev++.h>
#include <sd-daemon.h>

#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h> // O_CLOEXEC
#include <limits.h>
#include <stdlib.h>

#include <execinfo.h> // backtrace(), backtrace_symbols_fd()
#include <ucontext.h> // ucontext
#include <sys/utsname.h>

#if defined(HAVE_ZLIB_H)
#	include <zlib.h>
#endif

#if defined(HAVE_BZLIB_H)
#	include <bzlib.h>
#endif

#if defined(HAVE_SYSLOG_H)
#	include <syslog.h>
#endif

#if defined(ENABLE_TCP_DEFER_ACCEPT)
#	include <netinet/tcp.h>
#endif

#if !defined(XZERO_NDEBUG)
#	define TRACE(n, msg...) XZERO_DEBUG("XzeroDaemon", (n), msg)
#else
#	define TRACE(n, msg...) /*!*/ ((void)0)
#endif

// {{{ helper
static std::string getcwd()
{
    char buf[1024];
    return std::string(::getcwd(buf, sizeof(buf)));
}

static std::string& gsub(std::string& buf, const std::string& src, const std::string& dst)
{
    std::size_t i = buf.find(src);

    while (i != std::string::npos) {
        buf.replace(i, src.size(), dst);
        i = buf.find(src);
    }

    return buf;
}

static std::string& gsub(std::string& buf, const std::string& src, int dst)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%d", dst);
    return gsub(buf, src, tmp);
}
// }}}

namespace x0d {

using namespace x0;

typedef XzeroPlugin *(*plugin_create_t)(XzeroDaemon*, const std::string&);

void wrap_log_error(XzeroDaemon* d, const char* cat, const std::string& msg)
{
    d->server()->log(Severity::error, "%s: %s", cat, msg.c_str());
}

XzeroDaemon* XzeroDaemon::instance_ = nullptr;

XzeroDaemon::XzeroDaemon(int argc, char *argv[]) :
    state_(XzeroState::Inactive),
    argc_(argc),
    argv_(argv),
    showGreeter_(false),
    configfile_(pathcat(SYSCONFDIR, "x0d.conf")),
    pidfile_(),
    user_(),
    group_(),
    logTarget_("file"),
    logFile_(pathcat(LOGDIR, "x0d.log")),
    logLevel_(Severity::info),
    instant_(),
    documentRoot_(),
    nofork_(false),
    systemd_(getppid() == 1 && sd_booted()),
    doguard_(false),
    dumpAST_(false),
    dumpIR_(false),
    dumpTargetCode_(false),
    optimizationLevel_(1),
    server_(nullptr),
    evFlags_(0),
    eventHandler_(nullptr),
    pluginDirectory_(PLUGINDIR),
    plugins_(),
    pluginLibraries_(),
    core_(nullptr),
    components_(),
    unit_(),
    program_(),
    main_(nullptr),
    setupApi_(),
    mainApi_()
{
    TRACE(2, "initializing");
    instance_ = this;
}

XzeroDaemon::~XzeroDaemon()
{
    TRACE(2, "deleting eventHandler");
    delete eventHandler_;
    eventHandler_ = nullptr;

    while (!plugins_.empty())
        unloadPlugin(plugins_[plugins_.size() - 1]->name());

    TRACE(2, "deleting plugin core");
    if (core_) {
        unregisterPlugin(core_);
        delete core_;
        core_ = nullptr;
    }

    main_ = nullptr;

    TRACE(2, "deleting server");
    delete server_;
    server_ = nullptr;

    instance_ = nullptr;

#if defined(HAVE_SYSLOG_H)
    x0::SyslogSink::close();
#endif
}

int XzeroDaemon::run()
{
    ::signal(SIGPIPE, SIG_IGN);

    unsigned generation = 1;
    if (const char* v = getenv("XZERO_UPGRADE")) {
        generation = atoi(v) + 1;
        unsetenv("XZERO_UPGRADE");
    }

#if defined(HAVE_SYSLOG_H)
    x0::SyslogSink::open("x0d", LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif

    if (!parse())
        return 1;

    eventHandler_ = new XzeroEventHandler(this, ev::default_loop(evFlags_));
    server_ = new x0::HttpServer(eventHandler_->loop(), generation);

    // Load core plugins
    registerPlugin(core_ = new XzeroCore(this));

    if (systemd_) {
        nofork_ = true;
        server_->setLogger(std::make_shared<x0::SystemdLogger>());
    } else {
        if (logTarget_ == "file") {
            if (!logFile_.empty()) {
                auto logger = std::make_shared<x0::FileLogger>(logFile_, [this]() {
                    return static_cast<time_t>(ev_now(server_->loop()));
                });
                if (logger->handle() < 0) {
                    fprintf(stderr, "Could not open log file '%s': %s\n",
                            logFile_.c_str(), strerror(errno));
                    return 1;
                }

                server_->setLogger(logger);
            } else {
                server_->setLogger(std::make_shared<x0::SystemLogger>());
            }
        } else if (logTarget_ == "console") {
            server_->setLogger(std::make_shared<x0::ConsoleLogger>());
        } else if (logTarget_ == "syslog") {
            server_->setLogger(std::make_shared<x0::SystemLogger>());
        } else if (logTarget_ == "systemd") {
            server_->setLogger(std::make_shared<x0::SystemdLogger>());
        }
    }

    server_->setLogLevel(logLevel_);

    if (!setupConfig()) {
        log(x0::Severity::error, "Could not start x0d.");
        return -1;
    }

    unsetenv("XZERO_LISTEN_FDS");

    if (!nofork_)
        daemonize();

    if (!createPidFile())
        return -1;

    if (group_.empty())
        group_ = user_;

    if (!drop_privileges(user_, group_))
        return -1;

    if (showGreeter_) {
        printf("\n\n"
            "\e[1;37m"
            "             XXXXXXXXXXX\n"
            " XX     XX   XX       XX\n"
            "  XX   XX    XX       XX\n"
            "   XX XX     XX       XX\n"
            "    XXX      XX   0   XX - Web Server\n"
            "   XX XX     XX       XX   Version " PACKAGE_VERSION "\n"
            "  XX   XX    XX       XX\n"
            " XX     XX   XX       XX\n"
            "             XXXXXXXXXXX\n"
            "\n"
            " " PACKAGE_HOMEPAGE_URL
            "\e[0m"
            "\n\n"
        );
    }

    eventHandler_->setState(XzeroState::Running);

    int rv = server_->run();

    // remove PID-file, if exists and not in systemd-mode
    if (!systemd_ && !pidfile_.empty())
        unlink(pidfile_.c_str());

    return rv;
}

bool XzeroDaemon::parse()
{
    struct option long_options[] = {
        { "no-fork", no_argument, &nofork_, 1 },
        { "fork", no_argument, &nofork_, 0 },
        { "systemd", no_argument, &systemd_, 1 },
        { "guard", no_argument, &doguard_, 'G' },
        { "pid-file", required_argument, nullptr, 'p' },
        { "user", required_argument, nullptr, 'u' },
        { "group", required_argument, nullptr, 'g' },
        { "log-target", required_argument, nullptr, 'o' },
        { "log-file", required_argument, nullptr, 'l' },
        { "log-severity", required_argument, nullptr, 's' },
        { "event-loop", required_argument, nullptr, 'e' },
        { "instant", required_argument, nullptr, 'i' },
        { "dump-ast", no_argument, &dumpAST_, 1 },
        { "dump-ir", no_argument, &dumpIR_, 1 },
        { "dump-tc", no_argument, &dumpTargetCode_, 1 },
        //.
        { "splash", no_argument, nullptr, 'S' },
        { "version", no_argument, nullptr, 'v' },
        { "copyright", no_argument, nullptr, 'y' },
        { "config", required_argument, nullptr, 'f' },
        { "optimization-level", required_argument, &optimizationLevel_, 'O' },
        { "info", no_argument, nullptr, 'V' },
        { "help", no_argument, nullptr, 'h' },
        { "crash-handler", no_argument, nullptr, 'k' },
        //.
        { 0, 0, 0, 0 }
    };

    static const char *package_header = 
        "x0d: Xzero HTTP Web Server, version " PACKAGE_VERSION " [" PACKAGE_HOMEPAGE_URL "]";
    static const char *package_copyright =
        "Copyright (c) 2009-2014 by Christian Parpart <trapni@gmail.com>";
    static const char *package_license =
        "Licensed under AGPL-3 [http://gnu.org/licenses/agpl-3.0.txt]";

    for (;;) {
        int long_index = 0;
        switch (getopt_long(argc_, argv_, "vyf:O:p:u:g:o:l:s:i:e:khXGV", long_options, &long_index)) {
            case 'e': {
                char* saveptr = nullptr;
                char* input = optarg;
                std::vector<char*> items;
                for (;; input = nullptr) {
                    if (char* token = strtok_r(input, ",:", &saveptr))
                        items.push_back(token);
                    else
                        break;
                }
                for (auto item: items) {
                    if (strcmp(item, "select") == 0)
                        evFlags_ |= EVBACKEND_SELECT;
                    else if (strcmp(item, "poll") == 0)
                        evFlags_ |= EVBACKEND_POLL;
                    else if (strcmp(item, "epoll") == 0)
                        evFlags_ |= EVBACKEND_EPOLL;
                    else if (strcmp(item, "kqueue") == 0)
                        evFlags_ |= EVBACKEND_KQUEUE;
                    else if (strcmp(item, "devpoll") == 0)
                        evFlags_ |= EVBACKEND_DEVPOLL;
                    else if (strcmp(item, "port") == 0)
                        evFlags_ |= EVBACKEND_PORT;
                    else {
                        printf("Unknown event backend passed: '%s'.\n", item);
                        return false;
                    }
                }
                break;
            }
            case 'k':
                installCrashHandler();
                break;
            case 'S':
                showGreeter_ = true;
                break;
            case 'p':
                pidfile_ = optarg;
                break;
            case 'f':
                configfile_ = optarg;
                break;
            case 'O':
                optimizationLevel_ = std::atoi(optarg);
                break;
            case 'g':
                group_ = optarg;
                break;
            case 'u':
                user_ = optarg;
                break;
            case 'o':
                logTarget_ = optarg;

                if (logTarget_ != "file" && logTarget_ != "console" && logTarget_ != "syslog" && logTarget_ != "systemd") {
                    fprintf(stderr, "Invalid log target passed.\n");
                    return false;
                }
                break;
            case 'l':
                logFile_ = optarg;
                break;
            case 's':
                if (!logLevel_.set(optarg)) {
                    fprintf(stderr, "Invalid --log-severity value passed.\n");
                    return false;
                }
                break;
            case 'i':
                instant_ = optarg;
                logTarget_ = "console";
                nofork_ = true;
                break;
            case 'v':
                std::cout
                    << package_header << std::endl
                    << package_copyright << std::endl;
            case 'y':
                std::cout << package_license << std::endl;
                return false;
            case 'h':
                std::cout
                    << package_header << std::endl
                    << package_copyright << std::endl
                    << package_license << std::endl
                    << std::endl
                    << "usage:" << std::endl
                    << "  x0d [options ...]" << std::endl
                    << std::endl
                    << "options:" << std::endl
                    << "  -h,--help                 print this help" << std::endl
                    << "  -f,--config=PATH          specify a custom configuration file [" << configfile_ << "]" << std::endl
                    << "  -O,--optimization-level=N sets the configuration optimization level [" << optimizationLevel_ << "]" << std::endl
                    << "  -X,--no-fork              do not fork into background" << std::endl
                    << "     --systemd              force systemd-mode, which is auto-detected otherwise" << std::endl
                    << "  -G,--guard                do run service as child of a special guard process to watch for crashes" << std::endl
                    << "  -p,--pid-file=PATH        PID file to create" << std::endl
                    << "  -u,--user=NAME            user to drop privileges to" << std::endl
                    << "  -g,--group=NAME           group to drop privileges to" << std::endl
                    << "  -o,--log-target=TARGET    log target, one of: file, console, syslog, systemd [file]" << std::endl
                    << "  -l,--log-file=PATH        path to log file (ignored when log-target is not file)" << std::endl
                    << "  -s,--log-severity=VALUE   log severity level, one of: error, warning, notice, info, debug, debug1..6 [" << logLevel_.c_str() << "]" << std::endl
                    << "  -i,--instant=PATH[,PORT]  run x0d in simple pre-configured instant-mode,\n"
                    << "                            also implies --no-fork and --log-target=console" << std::endl
                    << "  -k,--crash-handler        installs SIGSEGV crash handler to print backtrace onto stderr." << std::endl
                    << "     --dump-ast             dumps parsed AST of the configuration file (for debugging purposes)" << std::endl
                    << "     --dump-ir              dumps IR of the configuration file (for debugging purposes)" << std::endl
                    << "     --dump-tc              dumps target code of the configuration file (for debugging purposes)" << std::endl
                    << "  -v,--version              print software version" << std::endl
                    << "  -y,--copyright            print software copyright notice / license" << std::endl
                    << "     --splash               print splash greeter to terminal on startup" << std::endl
                    << "  -V,--info                 print software version and compile-time enabled core features" << std::endl
                    << std::endl;
                return false;
            case 'V': {
                std::vector<std::string> features;

#if defined(TCP_DEFER_ACCEPT) && defined(ENABLE_TCP_DEFER_ACCEPT)
                features.push_back("+TCP_DEFER_ACCEPT");
#else
                features.push_back("-TCP_DEFER_ACCEPT");
#endif

#if defined(HAVE_ACCEPT4) && defined(ENABLE_ACCEPT4)
                features.push_back("+ACCEPT4");
#else
                features.push_back("-ACCEPT4");
#endif

                // TODO compile-time check for splice() and provide fallback.
                features.push_back("+SPLICE");

#if defined(HAVE_SYS_INOTIFY_H)
                features.push_back("+INOTIFY");
#else
                features.push_back("-INOTIFY");
#endif

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
                features.push_back("+FLOW_DIRECT_THREADED");
#else
                features.push_back("-FLOW_DIRECT_THREADED");
#endif

                std::cout
                    << package_header << std::endl
                    << package_copyright << std::endl;

                std::cout << "Features:";
                for (size_t i = 0, e = features.size(); i != e; ++i) {
                    std::cout << ' ' << features[i];
                }
                std::cout << std::endl;

                std::cout << "Plugin directory: " << PLUGINDIR << std::endl;

                return false;
            }
            case 'X':
                nofork_ = true;
                break;
            case 'G':
                doguard_ = true;
                break;
            case 0: // long option with (val!=nullptr && flag=0)
                break;
            case -1: { // EOF - everything parsed.
                if (optind == argc_)
                    return true;

                x0::Buffer args;
                while (optind < argc_)
                    args.printf(" %s", argv_[optind++]);

                std::fprintf(stderr, "Unknown trailing parameters:%s\n", args.c_str());
                // fall through
            }
            case '?': // ambiguous match / unknown arg
            default:
                return false;
        }
    }
}

void XzeroDaemon::daemonize()
{
    if (::daemon(true /*no chdir*/, true /*no close*/) < 0) {
        throw std::runtime_error(x0::fstringbuilder::format("Could not daemonize process: %s", strerror(errno)));
    }
}

/** drops runtime privileges current process to given user's/group's name. */
bool XzeroDaemon::drop_privileges(const std::string& username, const std::string& groupname)
{
    if (!groupname.empty() && !getgid()) {
        if (struct group *gr = getgrnam(groupname.c_str())) {
            if (setgid(gr->gr_gid) != 0) {
                log(Severity::error, "could not setgid to %s: %s", groupname.c_str(), strerror(errno));
                return false;
            }

            setgroups(0, nullptr);

            if (!username.empty()) {
                initgroups(username.c_str(), gr->gr_gid);
            }
        } else {
            log(Severity::error, "Could not find group: %s", groupname.c_str());
            return false;
        }
        TRACE(1, "Dropped group privileges to '%s'.", groupname.c_str());
    }

    if (!username.empty() && !getuid()) {
        if (struct passwd *pw = getpwnam(username.c_str())) {
            if (setuid(pw->pw_uid) != 0) {
                log(Severity::error, "could not setgid to %s: %s", username.c_str(), strerror(errno));
                return false;
            }
            log(Severity::info, "Dropped privileges to user %s", username.c_str());

            if (chdir(pw->pw_dir) < 0) {
                log(Severity::error, "could not chdir to %s: %s", pw->pw_dir, strerror(errno));
                return false;
            }
        } else {
            log(Severity::error, "Could not find group: %s", groupname.c_str());
            return false;
        }

        TRACE(1, "Dropped user privileges to '%s'.", username.c_str());
    }

    if (!::getuid() || !::geteuid() || !::getgid() || !::getegid()) {
#if defined(X0_RELEASE)
        log(x0::Severity::error, "Service is not allowed to run with administrative permissionsService is still running with administrative permissions.");
        return false;
#else
        log(x0::Severity::warn, "Service is still running with administrative permissions.");
#endif
    }
    return true;
}

void XzeroDaemon::log(Severity severity, const char *msg, ...)
{
    va_list va;
    char buf[4096];

    va_start(va, msg);
    vsnprintf(buf, sizeof(buf), msg, va);
    va_end(va);

    server_->log(severity, "%s", buf);
}

bool XzeroDaemon::setupConfig()
{
    if (instant_.empty()) {
        // this is no instant-mode, so setup via configuration file.
        return setup(std::make_unique<std::ifstream>(configfile_), configfile_, optimizationLevel_);
    }

    // --instant=docroot[,port[,bind]]

    auto tokens = x0::Tokenizer<x0::BufferRef, x0::Buffer>::tokenize(instant_, ",");
    documentRoot_ = tokens.size() > 0 ? tokens[0].str() : "";
    int port = tokens.size() > 1 ? tokens[1].toInt() : 0;
    std::string bind = tokens.size() > 2 ? tokens[2].str() : "";

    if (documentRoot_.empty())
        documentRoot_ = getcwd();
    else {
        char result[PATH_MAX];
        if (!realpath(documentRoot_.c_str(), result)) {
            log(x0::Severity::error, "Could not resolv document root: '%s'. %s", documentRoot_.c_str(), strerror(errno));
            return false;
        } else {
            documentRoot_ = result;
        }
    }

    if (!port)
        port = 8080;

    if (bind.empty())
        bind = "::"; //"0.0.0.0"; //TODO: "0::0";

    log(x0::Severity::debug, "docroot: %s", documentRoot_.c_str());
    log(x0::Severity::debug, "listen: addr=%s, port=%d", bind.c_str(), port);

    std::string source(
        "import compress;\n"
        "import dirlisting;\n"
        "import cgi;\n"
        "\n"
        "handler setup {\n"
        "  mimetypes '/etc/mime.types';\n"
        "  mimetypes.default 'application/octet-stream';\n"
        "  listen address: #{bind}, port: #{port};\n"
        "}\n"
        "\n"
        "handler main {\n"
        "  docroot '#{docroot}';\n"
        "  autoindex ['index.cgi', 'index.html'];\n"
        "  cgi.exec() if phys.path =$ '.cgi';\n"
        "  dirlisting;\n"
        "  staticfile;\n"
        "}\n"
    );
    gsub(source, "#{docroot}", documentRoot_);
    gsub(source, "#{bind}", bind); // FIXME <--- bind to 1 instead of "::" lol
    gsub(source, "#{port}", port);

    log(x0::Severity::debug2, "source: %s", source.c_str());

    server_->tcpCork(true);

    return setup(std::make_unique<std::istringstream>(source), "instant-mode.conf", optimizationLevel_);
}

bool XzeroDaemon::createPidFile()
{
    if (systemd_) {
        if (!pidfile_.empty())
            log(x0::Severity::info, "PID file requested but process is being supervised by systemd. Ignoring.");

        return true;
    }

    if (pidfile_.empty()) {
        if (nofork_) {
            return true;
        } else {
            log(x0::Severity::error, "No PID file specified. Use %s --pid-file=PATH.", argv_[0]);
            return false;
        }
    }

    FILE *pidfile = fopen(pidfile_.c_str(), "w");
    if (!pidfile) {
        log(x0::Severity::error, "Could not create PID file %s: %s.", pidfile_.c_str(), strerror(errno));
        return false;
    }

    fprintf(pidfile, "%d\n", getpid());
    fclose(pidfile);

    log(x0::Severity::info, "Created PID file with value %d [%s]", getpid(), pidfile_.c_str());

    return true;
}

/** starts new binary with (new) config - as child process, and gracefully shutdown self.
 */
void XzeroDaemon::reexec()
{
    if (state_ != XzeroState::Running) {
        server_->log(x0::Severity::info, "Reexec requested again?. Ignoring.");
        return;
    }

    eventHandler_->setState(XzeroState::Upgrading);

    // suspend worker threads while performing the reexec
    for (x0::HttpWorker* worker: server_->workers()) {
        worker->suspend();
    }

    x0::Buffer serializedListeners;

    for (x0::ServerSocket* listener: server_->listeners()) {
        // stop accepting new connections
        listener->stop();

        // and clear O_CLOEXEC on listener socket, as we want to probably resume these listeners in the child process
        listener->setCloseOnExec(false);

        serializedListeners.push_back(listener->serialize());
        serializedListeners.push_back(';');
    }

    server_->log(x0::Severity::debug, "Setting envvar XZERO_LISTEN_FDS to: '%s'", serializedListeners.c_str());
    setenv("XZERO_LISTEN_FDS", serializedListeners.c_str(), true);

    // prepare environment for new binary
    char sgen[20];
    snprintf(sgen, sizeof(sgen), "%u", server_->generation());
    setenv("XZERO_UPGRADE", sgen, true);

    std::vector<const char*> args;
    args.push_back(argv_[0]);

    if (systemd_)
        args.push_back("--systemd");

    if (!instant_.empty()) {
        args.push_back("--instant");
        args.push_back(instant_.c_str());
    } else {
        args.push_back("-f");
        args.push_back(configfile_.c_str());
    }

    args.push_back("--log-target");
    args.push_back(logTarget_.c_str());

    if (!logFile_.empty() && instant_.empty()) {
        args.push_back("--log-file");
        args.push_back(logFile_.c_str());
    }

    args.push_back("--log-severity");
    char logLevel[16];
    snprintf(logLevel, sizeof(logLevel), "%d", static_cast<int>(logLevel_));
    args.push_back(logLevel);

    if (!pidfile_.empty()) {
        args.push_back("--pid-file");
        args.push_back(pidfile_.c_str());
    }

    args.push_back("--no-fork"); // we never fork (potentially again)
    args.push_back(nullptr);

    int childPid = vfork();
    switch (childPid) {
        case 0:
            // in child
            execve(argv_[0], (char**)args.data(), environ);
            server_->log(x0::Severity::error, "Executing new child process failed: %s", strerror(errno));
            abort();
            break;
        case -1:
            // error
            server_->log(x0::Severity::error, "Forking for new child process failed: %s", strerror(errno));
            break;
        default:
            // in parent
            // the child process must tell us whether to gracefully shutdown or to resume.
            eventHandler_->setupChild(childPid);

            // we lost ownership of the PID file, if we had one as the child overwrites it.
            pidfile_ = "";

            // FIXME do we want a reexecTimeout, to handle possible cases where the child is not calling back? to kill them, if so!
            break;
    }

    // continue running the the process (with listeners disabled)
    server_->log(x0::Severity::debug, "Setting O_CLOEXEC on listener sockets");
    for (auto listener: server_->listeners()) {
        listener->setCloseOnExec(true);
    }
}

void XzeroDaemon::cycleLogs()
{
    for (auto plugin: plugins_)
        plugin->cycleLogs();

    server_->logger()->cycle();
}

// {{{ crash handler
void crashHandler(int nr, siginfo_t* info, void* ucp)
{
    ucontext* uc = static_cast<ucontext*>(ucp);

    void* addresses[256];
    int n = backtrace(addresses, sizeof(addresses) / sizeof(*addresses));

#if defined(__x86_64__)
    unsigned char* pc = reinterpret_cast<unsigned char*>(uc->uc_mcontext.gregs[REG_RIP]);
    fprintf(stderr, "Received SIGSEGV at %p.\n", pc);
#elif defined(__i386__)
    unsigned char* pc = reinterpret_cast<unsigned char*>(uc->uc_mcontext.gregs[REG_EIP]);
    fprintf(stderr, "Received SIGSEGV at %p.\n", pc);
#else
    fprintf(stderr, "Received SIGSEGV.\n");
#endif

    backtrace_symbols_fd(addresses, n, STDERR_FILENO);

    abort();
}

void XzeroDaemon::installCrashHandler()
{
    static bool installed = false;

    if (installed)
        return;

    installed = true;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &crashHandler;

    if (sigaction(SIGSEGV, &sa, NULL) < 0) {
        fprintf(stderr, "Could not install crash handler. %s\n", strerror(errno));
    }
}
// }}}
// {{{ plugin management
std::string XzeroDaemon::pluginDirectory() const
{
    return pluginDirectory_;
}

void XzeroDaemon::setPluginDirectory(const std::string& value)
{
    pluginDirectory_ = value;
}

/**
 * loads a plugin into the server.
 *
 * \see plugin, unload_plugin(), loaded_plugins()
 */
XzeroPlugin *XzeroDaemon::loadPlugin(const std::string& name, std::error_code& ec)
{
    for (XzeroPlugin* plugin: plugins_) {
        if (plugin->name() == name) {
            return plugin;
        }
    }

    if (!pluginDirectory_.empty() && pluginDirectory_[pluginDirectory_.size() - 1] != '/')
        pluginDirectory_ += "/";

    std::string filename;
    if (name.find('/') != std::string::npos)
        filename = name + ".so";
    else
        filename = pluginDirectory_ + name + ".so";

    std::string plugin_create_name("x0plugin_init");

#if !defined(XZERO_NDEBUG)
    log(Severity::debug, "Loading plugin %s", filename.c_str());
#endif

    Library lib;
    ec = lib.open(filename);
    if (!ec)
    {
        plugin_create_t plugin_create = reinterpret_cast<plugin_create_t>(lib.resolve(plugin_create_name, ec));

        if (!ec)
        {
            XzeroPlugin *plugin = plugin_create(this, name);
            pluginLibraries_[plugin] = std::move(lib);

            return registerPlugin(plugin);
        }
    }

    return nullptr;
}

/** safely unloads a plugin. */
void XzeroDaemon::unloadPlugin(const std::string& name)
{
    log(Severity::debug, "Unloading plugin: %s", name.c_str());

    for (auto plugin: plugins_) {
        if (plugin->name() == name) {
            unregisterPlugin(plugin);

            auto m = pluginLibraries_.find(plugin);
            if (m != pluginLibraries_.end()) {
                delete plugin;
                m->second.close();
                pluginLibraries_.erase(m);
            }
            return;
        }
    }
}

bool XzeroDaemon::pluginLoaded(const std::string& name) const
{
    for (const XzeroPlugin* plugin: plugins_) {
        if (plugin->name() == name) {
            return true;
        }
    }

    return false;
}

/** retrieves a list of currently loaded plugins */
std::vector<std::string> XzeroDaemon::pluginsLoaded() const
{
    std::vector<std::string> result;

    for (auto plugin: plugins_)
        result.push_back(plugin->name());

    return result;
}

XzeroPlugin *XzeroDaemon::registerPlugin(XzeroPlugin *plugin)
{
    plugins_.push_back(plugin);

    return plugin;
}

XzeroPlugin *XzeroDaemon::unregisterPlugin(XzeroPlugin *plugin)
{
    TRACE(1, "unregisterPlugin(\"%s\")", plugin->name().c_str());
    auto i = std::find(plugins_.begin(), plugins_.end(), plugin);
    if (i != plugins_.end()) {
        plugins_.erase(i);
    }

    return plugin;
}

void XzeroDaemon::addComponent(const std::string& value)
{
    components_.push_back(value);
}
// }}}
// {{{ Flow helper
bool XzeroDaemon::import(const std::string& name, const std::string& path, std::vector<FlowVM::NativeCallback*>* builtins)
{
    if (pluginLoaded(name))
        return true;

    std::string filename = path;
    if (!filename.empty() && filename[filename.size() - 1] != '/')
        filename += "/";
    filename += name;

    std::error_code ec;
    XzeroPlugin* plugin = loadPlugin(filename, ec);

    if (ec) {
        log(Severity::error, "Error loading plugin: %s: %s", filename.c_str(), ec.message().c_str());
        return false;
    }

    TRACE(1, "import(\"%s\", \"%s\", @%p)", name.c_str(), path.c_str(), builtins);
    if (builtins) {
        for (x0::FlowVM::NativeCallback* native: plugin->natives_) {
            TRACE(2, "  native name: %s", native->signature().to_s().c_str());
            builtins->push_back(native);
        }
    }

    return true;
}
// }}}
// {{{ configuration management
bool XzeroDaemon::setup(const std::string& filename, int optimizationLevel)
{
    std::unique_ptr<std::ifstream> s(new std::ifstream(filename));
    return setup(std::move(s), filename, optimizationLevel);
}

bool XzeroDaemon::setup(std::unique_ptr<std::istream>&& settings, const std::string& filename, int optimizationLevel)
{
    TRACE(1, "setup(%s)", filename.c_str());

    FlowParser parser(this);
    parser.importHandler = std::bind(&XzeroDaemon::import, this, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3);

    if (!parser.open(filename, std::move(settings))) {
        sd_notifyf(0, "ERRNO=%d", errno);
        fprintf(stderr, "Failed to open file: %s\n", filename.c_str());
        return false;
    }

    unit_ = parser.parse();
    if (!unit_)
        return false;

    if (dumpAST_)
        ASTPrinter::print(unit_.get());

    IRGenerator irgen;
    irgen.setExports({"setup", "main"});
    irgen.setErrorCallback([this](const std::string& message) {
        log(Severity::error, "[IR Generator] %s", message.c_str());
    });

    std::unique_ptr<IRProgram> ir = irgen.generate(unit_.get());
    if (!ir) {
        log(Severity::error, "IR Generator failed. Aborting.");
        return false;
    }

    {
        PassManager pm;

        // mandatory passes
        pm.registerPass(std::make_unique<UnusedBlockPass>());

        // optional passes
        if (optimizationLevel >= 1) {
            pm.registerPass(std::make_unique<EmptyBlockElimination>());
            pm.registerPass(std::make_unique<InstructionElimination>());
        }

        pm.run(ir.get());
    }

    if (!verify(ir.get())) {
        fprintf(stderr, "IR User verification failed.\n");
        return false;
    }

    if (dumpIR_) {
        ir->dump();
    }

    program_ = TargetCodeGenerator().generate(ir.get());

    ir.reset();

    if (!program_) {
        fprintf(stderr, "Code generation failed. Aborting.\n");
        return false;
    }

    if (!program_->link(this)) {
        fprintf(stderr, "Program linking failed. Aborting.\n");
        return false;
    }

    if (!validateConfig()) {
        return false;
    }

    if (dumpTargetCode_)
        program_->dump();

    // run setup
    TRACE(1, "run 'setup'");
    if (program_->findHandler("setup")->run(nullptr))
        // should not return true
        return false;

    // grap the request handler
    TRACE(1, "get pointer to 'main'");

    {
        auto main = program_->findHandler("main");

        server_->requestHandler = [=](x0::HttpRequest* r) {
            FlowVM::Runner* cx = static_cast<FlowVM::Runner*>(r->setCustomData(r, main->createRunner()));
            cx->setUserData(r);
            bool handled = cx->run();
            if (!cx->isSuspended() && !handled) {
                r->finish();
            }
        };
    }

    // {{{ setup server-tag
    {
#if defined(HAVE_SYS_UTSNAME_H)
        {
            utsname utsname;
            if (uname(&utsname) == 0) {
                addComponent(std::string(utsname.sysname) + "/" + utsname.release);
                addComponent(utsname.machine);
            }
        }
#endif

#if defined(HAVE_BZLIB_H)
        {
            std::string zver("bzip2/");
            zver += BZ2_bzlibVersion();
            zver = zver.substr(0, zver.find(","));
            addComponent(zver);
        }
#endif

#if defined(HAVE_ZLIB_H)
        {
            std::string zver("zlib/");
            zver += zlib_version;
            addComponent(zver);
        }
#endif

        Buffer tagbuf;
        tagbuf.push_back("x0/" VERSION);

        if (!components_.empty())
        {
            tagbuf.push_back(" (");

            for (int i = 0, e = components_.size(); i != e; ++i)
            {
                if (i)
                    tagbuf.push_back(", ");

                tagbuf.push_back(components_[i]);
            }

            tagbuf.push_back(")");
        }
        server_->tag = tagbuf.str();
    }
    // }}}

    // {{{ run post-config hooks
    TRACE(1, "setup: post_config");
    for (auto i: plugins_)
        if (!i->post_config())
            goto err;
    // }}}

    // {{{ run post-check hooks
    TRACE(1, "setup: post_check");
    for (auto i: plugins_)
        if (!i->post_check())
            goto err;
    // }}}

    // {{{ check for available TCP listeners
    if (server_->listeners().empty()) {
        log(Severity::error, "No HTTP listeners defined");
        goto err;
    }
    for (auto i: server_->listeners())
        if (!i->isOpen())
            goto err;
    // }}}

    // {{{ check for SO_REUSEPORT feature in TCP listeners
    if (server_->workers().size() == 1) {
        // fast-path scheduling for single-threaded mode
        server_->workers().front()->bind(server_->listeners().front());
    } else {
        std::list<ServerSocket*> dups;
        for (auto listener: server_->listeners()) {
            if (listener->reusePort()) {
                for (auto worker: server_->workers()) {
                    if (worker->id() > 0) {
                        // clone listener for non-main worker
                        listener = listener->clone(worker->loop());
                        dups.push_back(listener);
                    }
                    worker->bind(listener);
                }
            }
        }

        // FIXME: this is not yet well thought.
        // - how to handle configuration file reloads wrt SO_REUSEPORT?
        for (auto dup: dups) {
            server_->listeners().push_back(dup);
        }
    }
    // }}}

    // {{{ x0d: check for superfluous passed file descriptors (and close them)
    for (auto fd: ServerSocket::getInheritedSocketList()) {
        bool found = false;
        for (auto li: server_->listeners()) {
            if (fd == li->handle()) {
                found = true;
                break;
            }
        }
        if (!found) {
            log(Severity::debug, "Closing inherited superfluous listening socket %d.", fd);
            ::close(fd);
        }
    }
    // }}}

    // {{{ systemd: check for superfluous passed file descriptors
    if (int count = sd_listen_fds(0)) {
        int maxfd = SD_LISTEN_FDS_START + count;
        count = 0;
        for (int fd = SD_LISTEN_FDS_START; fd < maxfd; ++fd) {
            bool found = false;
            for (auto li: server_->listeners()) {
                if (fd == li->handle()) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ++count;
            }
        }
        if (count) {
            fprintf(stderr, "superfluous systemd file descriptors: %d\n", count);
            return false;
        }
    }
    // }}}

    // XXX post worker wakeup
    // we do an explicit wakeup of all workers here since there might be already
    // some (configure-time related) events pending, i.e. director's (fcgi) health checker
    // FIXME this is more a workaround than a fix.
    for (auto worker: server_->workers())
        worker->wakeup();

    TRACE(1, "setup: done.");
    return true;

err:
    return false;
}


bool XzeroDaemon::validateConfig()
{
    TRACE(1, "validateConfig()");

    auto setupFn = unit_->findHandler("setup");
    if (!setupFn) {
        log(Severity::error, "No setup-handler defined in config file.");
        return false;
    }

    auto mainFn = unit_->findHandler("main");
    if (!mainFn) {
        log(Severity::error, "No main-handler defined in config file.");
        return false;
    }

    TRACE(1, "validateConfig: setup:");

    FlowCallVisitor setupCalls(setupFn);
    if (!validate("setup", setupCalls.calls(), setupApi_))
        return false;

    FlowCallVisitor mainCalls(mainFn);
    if (!validate("main", mainCalls.calls(), mainApi_))
        return false;

    TRACE(1, "validateConfig finished");
    return true;
}

bool XzeroDaemon::validate(const std::string& context, const std::vector<CallExpr*>& calls, const std::vector<std::string>& api)
{
    for (const auto& i: calls) {
        if (!i->callee()->isBuiltin()) {
            // skip script user-defined handlers
            continue;
        }

        TRACE(1, " - %s (%ld args)", i->callee()->name().c_str(), i->args().size());

        if (std::find(api.begin(), api.end(), i->callee()->name()) == api.end()) {
            log(Severity::error, "Illegal call to '%s' found within %s-handler (or its callees).", i->callee()->name().c_str(), context.c_str());
            log(Severity::error, "%s", i->location().str().c_str());
            return false;
        }
    }
    return true;
}
// }}}

} // namespace x0d
