/* <x0/main.cpp>
 *
 * This file is part of the x0 web server project's daemon, called x0d
 * and is released under GPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/strutils.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>

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
		nofork_(false),
		doguard_(false),
		server_()
	{
		instance_ = this;
	}

	~x0d()
	{
		instance_ = 0;
	}

	int run(int argc, char *argv[])
	{
		if (parse(argc, argv))
		{
			server_.configure(configfile_);

			if (!nofork_)
			{
				daemonize();
			}

			if (doguard_)
			{
				return guard(boost::bind(&x0d::_run, this));
			}
			else
			{
				return _run();
			}
		}
		return -1;
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
				perror("fork");
				// TODO: syslog(strerror(errno))
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
				DEBUG("Waiting on pid %d", pid);
#if 0
				rv = waitpid(pid, &status, 0);
#else
				do rv = waitpid(pid, &status, 0);
				while (rv == -1 && errno == EINTR);
#endif
				perror("waitpid");

				if (WIFEXITED(status))
				{
					DEBUG("Guarded process terminated with return code %d", WEXITSTATUS(status));
					return WEXITSTATUS(status);
				}
				else if (WIFSIGNALED(status))
				{
					DEBUG("Guarded process terminated with signal %s", sig2str(WTERMSIG(status)).c_str());
					break;
				}
				else if (WIFSTOPPED(status))
					DEBUG("Guarded process stopped.");
				else if (WIFCONTINUED(status))
					DEBUG("Guarded process continued.");
			}
		}
#else
		return handler();
#endif
	}

	std::string sig2str(int sig)
	{
		static const char *sval[32] = {
			"SIGHUP",	// 1-5
			"SIGINT",
			"SIGQUIT",
			"SIGILL",
			0,
			"SIGABRT",	// 6-10
			0,
			"SIGFPE",
			"SIGKILL",
			0,
			"SIGSEGV",	// 11-15
			0,
			"SIGPIPE",
			"SIGALRM",
			"SIGUSR1",
			"SIGUSR2",
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

		server_.run();

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

		nofork_ = 0;

		for (;;)
		{
			int long_index = 0;
			switch (getopt_long(argc, argv, "vyc:hXG", long_options, &long_index))
			{
				case 'c':
					configfile_ = optarg;
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
						<< "   x0d [options ...]" << std::endl
						<< std::endl
						<< "options:" << std::endl
						<< "   -h,--help        print this help" << std::endl
						<< "   -c,--config=PATH specify a custom configuration file [" << configfile_ << "]" << std::endl
						<< "   -X,--no-fork     do not fork into background" << std::endl
						<< "   -G,--guard       do run service as child of a special guard process to watch for crashes" << std::endl
						<< "   -v,--version     print software version" << std::endl
						<< "   -y,--copyright   print software copyright notice / license" << std::endl
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

	static void reload_handler(int)
	{
		if (instance_)
		{
			instance_->server_.log(x0::severity::info, "SIGHUP received. Reloading configuration.");

			try
			{
				instance_->server_.reload();
			}
			catch (std::exception& e)
			{
				instance_->server_.log(x0::severity::error, "uncaught exception in reload handler: %s", e.what());
			}
		}
	}

	static void terminate_handler(int)
	{
		if (instance_)
		{
			instance_->server_.log(x0::severity::info, "SIGTERM received. Shutting down.");

			try
			{
				instance_->server_.stop();
			}
			catch (std::exception& e)
			{
				instance_->server_.log(x0::severity::error, "uncaught exception in terminate handler: %s", e.what());
			}
		}
	}

private:
	std::string configfile_;
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
