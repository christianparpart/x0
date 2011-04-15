/* <x0/x0d.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpCore.h>
#include <x0/flow/FlowRunner.h>
#include <x0/Logger.h>
#include <x0/strutils.h>
#include <x0/Severity.h>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include <sd-daemon.h>

#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>

#if !defined(NDEBUG)
#	define X0D_DEBUG(msg...) x0d::log(x0::Severity::debug, msg)
#else
#	define X0D_DEBUG(msg...) /*!*/ ((void)0)
#endif

using x0::Severity;

class x0d
{
private:
	/** concats a path with a filename and optionally inserts a path seperator if path 
	 *  doesn't contain a trailing path seperator. */
	static inline std::string pathcat(const std::string& path, const std::string& filename)
	{
		if (!path.empty() && path[path.size() - 1] != '/')
		{
			return path + "/" + filename;
		}
		else
		{
			return path + filename;
		}
	}

public:
	x0d(int argc, char *argv[]) :
		argc_(argc),
		argv_(argv),
		configfile_(pathcat(SYSCONFDIR, "x0d.conf")),
		pidfile_(),
		user_(),
		group_(),
		instant_(),
		documentRoot_(),
		nofork_(false),
		systemd_(sd_controlled()),
		doguard_(false),
		dumpIR_(false),
		server_(new x0::HttpServer()),
		sigterm_(server_->loop()),
		sighup_(server_->loop())
	{
		x0::FlowRunner::initialize();

#ifndef NDEBUG
		nofork_ = true;
		configfile_ = "../../../src/test.conf";
		server_->logLevel(x0::Severity::debug5);
#endif
		instance_ = this;

		sigterm_.set<x0d, &x0d::terminate_handler>(this);
		sigterm_.start(SIGTERM);

		sighup_.set<x0d, &x0d::reload_handler>(this);
		sighup_.start(SIGHUP);
	}

	~x0d()
	{
		delete server_;
		server_ = nullptr;

		instance_ = nullptr;

		x0::FlowRunner::shutdown();
	}

	static x0d *instance()
	{
		return instance_;
	}

	static std::string getcwd()
	{
		char buf[1024];
		return std::string(::getcwd(buf, sizeof(buf)));
	}

	static std::string& gsub(std::string& buf, const std::string& src, const std::string& dst)
	{
		std::size_t i = buf.find(src);
		while (i != std::string::npos)
		{
			buf.replace(i, src.size(), dst);
			i = buf.find(src);
		}
		return buf;
	}

	static std::string& gsub(std::string& buf, const std::string& src, int dst)
	{
		std::string tmp(boost::lexical_cast<std::string>(dst));
		return gsub(buf, src, tmp);
	}

	// --instant=docroot,port,bind
	bool setupInstantMode()
	{
		typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

		auto tokens = x0::split<std::string>(instant_, ",");
		documentRoot_ = tokens.size() > 0 ? tokens[0] : "";
		int port = tokens.size() > 1 ? boost::lexical_cast<int>(tokens[1]) : 0;
		std::string bind = tokens.size() > 2 ? tokens[2] : "";

		if (documentRoot_.empty())
			documentRoot_ = getcwd();

		if (!port)
			port = 8080;

		if (bind.empty())
			bind = "0.0.0.0"; //TODO: "0::0";

		std::string source(
			"import 'compress';\n"
			"import 'dirlisting';\n"
			"import 'cgi';\n"
			"\n"
			"handler setup\n"
			"{\n"
			"    listen '#{bind}:#{port}';\n"
			"    worker 1;\n"
			"}\n"
			"\n"
			"handler main\n"
			"{\n"
			"    docroot '#{docroot}';\n"
			"    autoindex ['index.cgi', 'index.html'];\n"
			"    cgi.exec if phys.path =$ '.cgi';\n"
			"    dirlisting;\n"
			"    staticfile;\n"
			"}\n"
		);
		gsub(source, "#{docroot}", documentRoot_);
		gsub(source, "#{bind}", bind);
		gsub(source, "#{port}", port);

		// initialize some default settings (fileinfo)
#if 0
		server_->fileinfo.load_mimetypes("/etc/mime.types");
		server_->fileinfo.default_mimetype("application/octet-stream");
		server_->fileinfo.etag_consider_mtime(true);
		server_->fileinfo.etag_consider_size(true);
		server_->fileinfo.etag_consider_inode(false);
#endif

		server_->tcpCork(true);

		std::istringstream s(source);
		return server_->setup(&s, SYSCONFDIR "/instant.conf");
	}

	int run()
	{
		if (!parse())
			return 1;

		if (systemd_) {
			nofork_ = true;
			server_->setLogger(std::make_shared<x0::SystemdLogger>());
		} else {
			server_->setLogger(std::make_shared<x0::SystemLogger>());
		}

		bool rv = false;
		if (!instant_.empty())
			rv = setupInstantMode();
		else {
			std::ifstream ifs(configfile_);
			rv = server_->setup(&ifs, configfile_);
		}

		if (!rv) {
			log(x0::Severity::error, "Could not start x0d.");
			return -1;
		}

		if (dumpIR_)
			server_->dumpIR();

		if (!nofork_)
			daemonize();

		if (group_.empty())
			group_ = user_;

		if (!drop_privileges(user_, group_))
			return -1;

		return doguard_
			? guard(std::bind(&x0d::_run, this))
			: _run();
	}

	template<class Handler>
	int guard(Handler handler)
	{
#if 1
		// TODO: fork and guard the child process, that is, restart the child on each time it ends abnormally
		for (;;)
		{
			pid_t pid = fork();

			if (pid < 0) // fork failed
			{
				log(x0::Severity::error, "fork error: %s", strerror(errno));
				return 1;
			}
			else if (pid == 0) // in child
			{
				return handler();
			}
			// in parent with a valid child
			int rv, status = 0;

			for (;;) // loop as long as waitpid() returned errno (EINTR, ...) or the program exited normally (exit(), main() leaving, ...)
			{
				X0D_DEBUG("Waiting on pid %d", pid);
#if 0
				rv = waitpid(pid, &status, 0);
#else
				do rv = waitpid(pid, &status, 0);
				while (rv == -1 && errno == EINTR);
#endif
				perror("waitpid");

				if (WIFEXITED(status))
				{
					X0D_DEBUG("Guarded process terminated with return code %d", WEXITSTATUS(status));
					return WEXITSTATUS(status);
				}
				else if (WIFSIGNALED(status))
				{
					X0D_DEBUG("Guarded process terminated with signal %s", sig2str(WTERMSIG(status)).c_str());
					break;
				}
				else if (WIFSTOPPED(status))
					X0D_DEBUG("Guarded process stopped.");
				else if (WIFCONTINUED(status))
					X0D_DEBUG("Guarded process continued.");
			}
		}
#else
		return handler();
#endif
	}

	static std::string sig2str(int sig)
	{
		static const char *sval[32] = {
			"SIGHUP",	// 1
			"SIGINT",	// 2
			"SIGQUIT",	// 3
			"SIGILL",	// 4
			0,			// 5
			"SIGABRT",	// 6
			0,			// 7
			"SIGFPE",	// 8
			"SIGKILL",	// 9
			0,			// 10
			"SIGSEGV",	// 11
			0,			// 12
			"SIGPIPE",	// 13
			"SIGALRM",	// 14
			"SIGTERM",	// 15
			"SIGUSR1",	// 16
			"SIGUSR2",	// 17
			"SIGCHLD",	// 18
			"SIGCONT",	// 19
			"SIGSTOP",	// 20
			0
		};

		char buf[64];
		snprintf(buf, sizeof(buf), "%s (%d)", sval[sig - 1], sig);
		return buf;
	}

	bool createPidFile()
	{
		if (systemd_) {
			if (!pidfile_.empty())
				log(x0::Severity::info, "PID file requested but process is being supervised by systemd. Ignoring.");

			return true;
		}

		if (pidfile_.empty()) {
			log(x0::Severity::warn, "No PID file specified. Use %s --pid-file=PATH.", argv_[0]);
			return false;
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

	int _run()
	{
		::signal(SIGPIPE, SIG_IGN);

		if (!createPidFile())
			return -1;

		int rv = server_->run();

		if (!systemd_ && !pidfile_.empty())
			unlink(pidfile_.c_str());

		return rv;
	}

private:
	bool parse()
	{
		struct option long_options[] =
		{
			{ "no-fork", no_argument, &nofork_, 1 },
			{ "fork", no_argument, &nofork_, 0 },
			{ "systemd", no_argument, &systemd_, 1 },
			{ "guard", no_argument, &doguard_, 'G' },
			{ "pid-file", required_argument, 0, 'p' },
			{ "user", required_argument, 0, 'u' },
			{ "group", required_argument, 0, 'g' },
			{ "instant", required_argument, 0, 'i' },
			{ "dump-ir", no_argument, &dumpIR_, 1 },
			//.
			{ "version", no_argument, 0, 'v' },
			{ "copyright", no_argument, 0, 'y' },
			{ "config", required_argument, 0, 'c' },
			{ "help", no_argument, 0, 'h' },
			//.
			{ 0, 0, 0, 0 }
		};

		static const char *package_header = 
			"x0d: x0 web server, version " PACKAGE_VERSION " [" PACKAGE_HOMEPAGE_URL "]";
		static const char *package_copyright =
			"Copyright (c) 2009 by Christian Parpart <trapni@gentoo.org>";
		static const char *package_license =
			"Licensed under GPL-3 [http://gplv3.fsf.org/]";

		for (;;)
		{
			int long_index = 0;
			switch (getopt_long(argc_, argv_, "vyc:p:u:g:i:hXG", long_options, &long_index))
			{
				case 'p':
					pidfile_ = optarg;
					break;
				case 'c':
					configfile_ = optarg;
					break;
				case 'g':
					group_ = optarg;
					break;
				case 'u':
					user_ = optarg;
					break;
				case 'i':
					instant_ = optarg;
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
						<< "  -h,--help                print this help" << std::endl
						<< "  -c,--config=PATH         specify a custom configuration file [" << configfile_ << "]" << std::endl
						<< "  -X,--no-fork             do not fork into background" << std::endl
						<< "     --systemd             force systemd-mode, which is auto-detected otherwise" << std::endl
						<< "  -G,--guard               do run service as child of a special guard process to watch for crashes" << std::endl
						<< "  -p,--pid-file=PATH       PID file to create" << std::endl
						<< "  -u,--user=NAME           user to drop privileges to" << std::endl
						<< "  -g,--group=NAME          group to drop privileges to" << std::endl
						<< "     --dump-ir             dumps LLVM IR of the configuration file (for debugging purposes)" << std::endl
						<< "  -i,--instant=PATH[,PORT] run x0d in simple pre-configured instant-mode" << std::endl
						<< "  -v,--version             print software version" << std::endl
						<< "  -y,--copyright           print software copyright notice / license" << std::endl
						<< std::endl;
					return false;
				case 'X':
					nofork_ = true;
					break;
				case 'G':
					doguard_ = true;
					break;
				case 0: // long option with (val!=nullptr && flag=0)
					break;
				case -1: // EOF - everything parsed.
					return true;
				case '?': // ambiguous match / unknown arg
				default:
					return false;
			}
		}
	}

	void daemonize()
	{
		if (::daemon(true /*no chdir*/, true /*no close*/) < 0)
		{
			throw std::runtime_error(x0::fstringbuilder::format("Could not daemonize process: %s", strerror(errno)));
		}
	}

	/** drops runtime privileges current process to given user's/group's name. */
	bool drop_privileges(const std::string& username, const std::string& groupname)
	{
		if (!groupname.empty() && !getgid())
		{
			if (struct group *gr = getgrnam(groupname.c_str()))
			{
				if (setgid(gr->gr_gid) != 0) {
					log(Severity::error, "could not setgid to %s: %s", groupname.c_str(), strerror(errno));
					return false;
				}

				setgroups(0, nullptr);

				if (!username.empty())
					initgroups(username.c_str(), gr->gr_gid);
			}
			else
			{
				log(Severity::error, "Could not find group: %s", groupname.c_str());
				return false;
			}
			X0D_DEBUG("Dropped group privileges to '%s'.", groupname.c_str());
		}

		if (!username.empty() && !getuid())
		{
			if (struct passwd *pw = getpwnam(username.c_str()))
			{
				if (setuid(pw->pw_uid) != 0) {
					log(Severity::error, "could not setgid to %s: %s", username.c_str(), strerror(errno));
					return false;
				}

				if (chdir(pw->pw_dir) < 0) {
					log(Severity::error, "could not chdir to %s: %s", pw->pw_dir, strerror(errno));
					return false;
				}
			}
			else {
				log(Severity::error, "Could not find group: %s", groupname.c_str());
				return false;
			}

			X0D_DEBUG("Dropped user privileges to '%s'.", username.c_str());
		}

		if (!::getuid() || !::geteuid() || !::getgid() || !::getegid())
		{
#if defined(X0_RELEASE)
			log(x0::Severity::error, "Service is not allowed to run with administrative permissionsService is still running with administrative permissions.");
			return false;
#else
			log(x0::Severity::warn, "Service is still running with administrative permissions.");
#endif
		}
		return true;
	}

	static void log(Severity severity, const char *msg, ...)
	{
		va_list va;
		char buf[2048];

		va_start(va, msg);
		vsnprintf(buf, sizeof(buf), msg, va);
		va_end(va);

		if (instance_)
		{
			instance_->server_->log(severity, "%s", buf);
		}
		else
		{
			X0D_DEBUG("%s", buf);
		}
	}

	void reload_handler(ev::sig&, int)
	{
		log(x0::Severity::info, "SIGHUP received. Reloading configuration.");

		try
		{
			server_->reload();
		}
		catch (std::exception& e)
		{
			log(x0::Severity::error, "uncaught exception in reload handler: %s", e.what());
		}
	}

	void terminate_handler(ev::sig&, int)
	{
		log(x0::Severity::info, "SIGTERM received. Shutting down.");

		try
		{
			server_->stop();
		}
		catch (std::exception& e)
		{
			log(x0::Severity::error, "uncaught exception in terminate handler: %s", e.what());
		}
	}

private:
	int argc_;
	char **argv_;
	std::string configfile_;
	std::string pidfile_;
	std::string user_;
	std::string group_;

	std::string instant_;
	std::string documentRoot_;

	int nofork_;
	int systemd_;
	int doguard_;
	int dumpIR_;
	x0::HttpServer *server_;
	ev::sig sigterm_;
	ev::sig sighup_;
	static x0d *instance_;
};

x0d *x0d::instance_ = 0;

int main(int argc, char *argv[])
{
	try
	{
		x0d daemon(argc, argv);
		return daemon.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Unhandled exception caught: " << e.what() << std::endl;
		return 1;
	}
	catch (const char *e)
	{
		std::cerr << "Unhandled exception caught: " << e << std::endl;
		return 2;
	}
	catch (...)
	{
		std::cerr << "Unhandled unknown exception caught." << std::endl;
		return 3;
	}
}
