#include <x0/process.hpp>
#include <boost/asio.hpp>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

namespace x0 {

/** invokes \p cmd until its not early aborted with EINTR. */
#define EINTR_LOOP(rv, cmd) 				\
	do {									\
		rv = cmd;							\
	} while (rv == -1 && errno == EINTR)

process::process(boost::asio::io_service& io) :
	input_(io), output_(io), error_(io), pid_(-1), status_(0)
{
}

process::process(boost::asio::io_service& io, const std::string& exe, const params& args, const environment& env, const std::string& workdir) :
	input_(io), output_(io), error_(io), pid_(-1), status_(0)
{
	start(exe, args, env, workdir);
}

process::~process()
{
	// XXX kill child?
	fetch_status();
}

void process::start(const std::string& exe, const params& args, const environment& env, const std::string& workdir)
{
	//::fprintf(stderr, "proc[%d] start(exe=%s, args=[...], workdir=%s)\n", getpid(), exe.c_str(), workdir.c_str());
	switch (pid_ = fork())
	{
		case 0: // child
			setup_child(exe, args, env, workdir);
			break;
		case -1: // error
			throw boost::system::system_error(boost::system::error_code(errno, boost::system::system_category));
		default: // parent
			setup_parent();
			break;
	}
}

void process::terminate()
{
	::kill(pid_, SIGTERM);
}

bool process::expired()
{
	if (pid_ <= 0)
		return true;

	switch (fetch_status())
	{
		case 0:
			// we could fetch a status, meaning, program is still running
			return false;
		case -1:
			// oops? waitpid error?
			return false;
		default:
			// waidpid returned value > 0, meaning, program has returned already(?)
			return true;
	}
}

int process::fetch_status()
{
	int rv;

	do rv = ::waitpid(pid_, &status_, WNOHANG);
	while (rv == -1 && (errno == EINTR || errno == ECHILD));

	return rv;
}

void process::setup_parent()
{
	// setup I/O
	input_.remote().close();
	output_.remote().close();
	error_.remote().close();
}

void process::setup_child(const std::string& _exe, const params& _args, const environment& _env, const std::string& _workdir)
{
	// restore signal handler(s)
	::signal(SIGPIPE, SIG_DFL);

	// setup environment
	int k = 0;
	std::vector<char *> env(_env.size() + 1);

	for (environment::const_iterator i = _env.cbegin(), e = _env.cend(); i != e; ++i)
	{
		char *buf = new char[i->first.size() + i->second.size() + 2];
		::memcpy(buf, i->first.c_str(), i->first.size());
		buf[i->first.size()] = '=';
		::memcpy(buf + i->first.size() + 1, i->second.c_str(), i->second.size() + 1);

		//::fprintf(stderr, "proc[%d]: setting env[%d]: %s\n", getpid(), k, buf);
		//::fflush(stderr);
		env[k++] = buf;
	}
	env[_env.size()] = 0;

	// setup args
	std::vector<char *> args(_args.size() + 2);
	args[0] = const_cast<char *>(_exe.c_str());
	//::fprintf(stderr, "args[%d] = %s\n", 0, args[0]);
	for (int i = 0, e = _args.size(); i != e; ++i)
	{
		args[i + 1] = const_cast<char *>(_args[i].c_str());
		//::fprintf(stderr, "args[%d] = %s\n", i + 1, args[i + 1]);
	}
	args[args.size() - 1] = 0;

	// chdir
	if (!_workdir.empty())
	{
		::chdir(_workdir.c_str());
	}

	// setup I/O
	int rv;
	EINTR_LOOP(rv, ::close(STDIN_FILENO));
	EINTR_LOOP(rv, ::close(STDOUT_FILENO));
	EINTR_LOOP(rv, ::close(STDERR_FILENO));

	EINTR_LOOP(rv, ::dup2(input_.remote().native(), STDIN_FILENO));
	EINTR_LOOP(rv, ::dup2(output_.remote().native(), STDOUT_FILENO));
	EINTR_LOOP(rv, ::dup2(error_.remote().native(), STDERR_FILENO));

//	input_.close();
//	output_.close();
//	error_.close();

	// finally execute
	::execve(args[0], &args[0], &env[0]);

	// OOPS
	::fprintf(stderr, "proc[%d]: execve(%s) error: %s\n", getpid(), args[0], strerror(errno));
	::fflush(stderr);
	::_exit(1);
}

} // namespace x0
