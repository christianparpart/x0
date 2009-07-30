#include <x0/process.hpp>
#include <boost/asio.hpp>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

namespace x0 {

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
}

void process::start(const std::string& exe, const params& args, const environment& env, const std::string& workdir)
{
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
	// setup environment
	int k = 0;
	std::vector<char *> env(_env.size() + 1);

	for (environment::const_iterator i = _env.cbegin(), e = _env.cend(); i != e; ++i)
	{
		char *buf = new char[i->first.size() + i->second.size() + 2];
		::memcpy(buf, i->first.c_str(), i->first.size());
		buf[i->first.size()] = '=';
		::memcpy(buf + i->first.size() + 1, i->second.c_str(), i->second.size() + 1);

		printf("proc: setting env[%d]: %s\n", k, buf);
		env[k++] = buf;
	}
	env[_env.size()] = 0;

	// setup args
	std::vector<char *> args(_args.size() + 2);
	args[0] = const_cast<char *>(_exe.c_str());
	for (int i = 0, e = _args.size(); i != e; ++i)
		args[i + 1] = const_cast<char *>(_args[i].c_str());
	args[args.size() - 1] = 0;

	// chdir
	if (!_workdir.empty())
	{
		::chdir(_workdir.c_str());
	}

	// setup I/O
	::close(STDIN_FILENO);
	::dup2(input_.remote().native(), STDIN_FILENO);
	input_.close();

	::close(STDOUT_FILENO);
	::dup2(output_.remote().native(), STDOUT_FILENO);
	output_.close();

	::close(STDERR_FILENO);
	::dup2(error_.remote().native(), STDERR_FILENO);
	error_.close();

	// finally execute
	::execve(args[0], &args[0], &env[0]);

	// OOPS
	::printf("execve(%s) error: %s\n", args[0], strerror(errno));
	::_exit(1);
}

} // namespace x0
