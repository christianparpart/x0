/* <x0/main.cpp>
 *
 * This file is part of the x0 web server project's daemon, called x0d
 * and is released under GPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/strutils.hpp>
#include <x0/severity.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>

#if !defined(NDEBUG)
#	define X0D_DEBUG(msg...) x0d::log(x0::severity::debug, msg)
#else
#	define X0D_DEBUG(msg...) /*!*/ ((void)0)
#endif

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
	x0d() :
		configfile_(pathcat(SYSCONFDIR, "x0d.conf")),
		pidfile_(pathcat(LOCALSTATEDIR, "run/x0d.pid")),
		user_(""),
		group_(""),
		nofork_(false),
		doguard_(false),
		server_()
	{
#ifndef NDEBUG
		nofork_ = true;
		configfile_ = "test.conf";
#endif
		instance_ = this;
	}

	~x0d()
	{
		instance_ = 0;
	}

	static x0d *instance()
	{
		return instance_;
	}

	int run(int argc, char *argv[])
	{
		if (!parse(argc, argv))
			return 1;

		server_.configure(configfile_);

		if (!nofork_)
			daemonize();

		if (user_.empty())
			user_ = server_.config()["Daemon"]["User"].as<std::string>();

		if (group_.empty())
			group_ = server_.config()["Daemon"]["Group"].as<std::string>();

		drop_privileges(user_, group_);

		return doguard_
			? guard(boost::bind(&x0d::_run, this))
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
				log(x0::severity::alert, "fork error: %s", strerror(errno));
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

	int _run()
	{
		::signal(SIGHUP, &reload_handler);
		::signal(SIGTERM, &terminate_handler);
		::signal(SIGPIPE, SIG_IGN);

		if (FILE *pidfile = fopen(pidfile_.c_str(), "w"))
		{
			server_.log(x0::severity::info, "Created PID file with value %d [%s]", getpid(), pidfile_.c_str());
			fprintf(pidfile, "%d\n", getpid());
			fclose(pidfile);
		} else
			server_.log(x0::severity::error, "Could not create PID file: %s.", strerror(errno));

		server_.run();

		unlink(pidfile_.c_str());

		return 0;
	}

private:
	bool parse(int argc, char *argv[])
	{
		struct option long_options[] =
		{
			{ "no-fork", no_argument, &nofork_, 1 },
			{ "fork", no_argument, &nofork_, 0 },
			{ "guard", no_argument, &doguard_, 'G' },
			{ "pid-file", required_argument, 0, 'p' },
			{ "user", required_argument, 0, 'u' },
			{ "group", required_argument, 0, 'g' },
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
			switch (getopt_long(argc, argv, "vyc:hXG", long_options, &long_index))
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
						<< "  -h,--help           print this help" << std::endl
						<< "  -c,--config=PATH    specify a custom configuration file [" << configfile_ << "]" << std::endl
						<< "  -X,--no-fork        do not fork into background" << std::endl
						<< "  -G,--guard          do run service as child of a special guard process to watch for crashes" << std::endl
						<< "  -p,--pid-file=PATH  PID file to create/use [" << pidfile_ << "]" << std::endl
						<< "     --user=NAME      user to drop privileges to" << std::endl
						<< "     --group=NAME     group to drop privileges to" << std::endl
						<< "  -v,--version        print software version" << std::endl
						<< "  -y,--copyright      print software copyright notice / license" << std::endl
						<< std::endl;
					return false;
				case 'X':
					nofork_ = true;
					break;
				case 'G':
					doguard_ = true;
					break;
				case 0: // long option with (val!=NULL && flag=0)
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
	void drop_privileges(const std::string& username, const std::string& groupname)
	{
		if (!groupname.empty() && !getgid())
		{
			if (struct group *gr = getgrnam(groupname.c_str()))
			{
				if (setgid(gr->gr_gid) != 0)
					throw std::runtime_error(x0::fstringbuilder::format("could not setgid to %s: %s", groupname.c_str(), strerror(errno)));
			}
			else
			{
				throw std::runtime_error(x0::fstringbuilder::format("Could not find group: %s", groupname.c_str()));
			}
			X0D_DEBUG("Dropped group privileges to '%s'.", groupname.c_str());
		}

		if (!username.empty() && !getuid())
		{
			if (struct passwd *pw = getpwnam(username.c_str()))
			{
				if (setuid(pw->pw_uid) != 0)
					throw std::runtime_error(x0::fstringbuilder::format("could not setgid to %s: %s", username.c_str(), strerror(errno)));

				if (chdir(pw->pw_dir) < 0)
					throw std::runtime_error(x0::fstringbuilder::format("could not chdir to %s: %s", pw->pw_dir, strerror(errno)));
			}
			else
				throw std::runtime_error(x0::fstringbuilder::format("Could not find group: %s", groupname.c_str()));

			X0D_DEBUG("Dropped user privileges to '%s'.", username.c_str());
		}

		if (!::getuid() || !::geteuid() || !::getgid() || !::getegid())
		{
#if defined(X0_RELEASE)
			throw std::runtime_error(x0::fstringbuilder::format("Service is not allowed to run with administrative permissionsService is still running with administrative permissions."));
#else
			log(x0::severity::warn, "Service is still running with administrative permissions.");
#endif
		}
	}

	static void log(x0::severity severity, const char *msg, ...)
	{
		va_list va;
		char buf[2048];

		va_start(va, msg);
		vsnprintf(buf, sizeof(buf), msg, va);
		va_end(va);

		if (instance_)
		{
			instance_->server_.log(severity, "%s", buf);
		}
		else
		{
			X0D_DEBUG("%s", buf);
		}
	}

	static void reload_handler(int signo)
	{
		if (instance_)
		{
			log(x0::severity::info, "%s received. Reloading configuration.", sig2str(signo).c_str());

			try
			{
				instance_->server_.reload();
			}
			catch (std::exception& e)
			{
				log(x0::severity::error, "uncaught exception in reload handler: %s", e.what());
			}
		}
	}

	static void terminate_handler(int signo)
	{
		if (instance_)
		{
			log(x0::severity::info, "%s received. Shutting down.", sig2str(signo).c_str());

			try
			{
				instance_->server_.stop();
			}
			catch (std::exception& e)
			{
				log(x0::severity::error, "uncaught exception in terminate handler: %s", e.what());
			}
		}
	}

private:
	std::string configfile_;
	std::string pidfile_;
	std::string user_;
	std::string group_;
	int nofork_;
	int doguard_;
	x0::server server_;
	static x0d *instance_;
};

x0d *x0d::instance_ = 0;

int main(int argc, char *argv[])
{
	try
	{
		x0d daemon;
		return daemon.run(argc, argv);
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
