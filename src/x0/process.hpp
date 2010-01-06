/* <x0/process.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_process_hpp
#define sw_x0_process_hpp 1

#include <x0/local_stream.hpp>
#include <x0/api.hpp>
#include <boost/noncopyable.hpp>
#include <asio.hpp>
#include <vector>
#include <map>
#include <string>

namespace x0 {

//! \addtogroup base
//@{

/** Creates, runs, and manages a child process running external programs.
 *
 * \note You may only run one child at a time per \p process <b>instance</b>.
 */
class process :
	public boost::noncopyable
{
public:
	/// vector list used for storing program parameters
	typedef std::vector<std::string> params;

	/// string map used for storing custom environment variables.
	typedef std::map<std::string, std::string> environment;

	/// stream type used for redirecting the child processes stdio (in/out/err)
	typedef local_stream::socket iostream;

public:
	/** initializes the process object without actually starting any child.
	 *
	 * \param io the io_service to use.
	 *
	 * The I/O communication however is already initialized.
	 * Use start() to actually start a child program.
	 *
	 * \see start(const std::string& exe, const params& args, const environment& env, const std::string& workdir)
	 */
	explicit process(asio::io_service& io);

	/** initializes this process object and actually starts a child program as specified.
	 *
	 * \param io the io_service to use.
	 * \param exe path to the executable in your local filesystem to be executed.
	 * \param args program parameters to be passed to this executable.
	 * \param env custom environment variables to be passed.
	 * \param workdir optional workdir to change to before the program inside the child process gets executed.
	 *
	 * \note you may only run one child at a time per process object.
	 */
	process(asio::io_service& io, const std::string& exe, const params& args,
		const environment& env = environment(), const std::string& workdir = std::string());

	~process();

	/** socket handle to the STDIN of the child. */
	iostream& input();

	/** socket handle to the STDOUT of the child. */
	iostream& output();

	/** socket handle to the STDERR of the child. */
	iostream& error();

	/** executes a program as a child process as specified.
	 *
	 * \param io the io_service to use.
	 * \param exe path to the executable in your local filesystem to be executed.
	 * \param args program parameters to be passed to this executable.
	 * \param env custom environment variables to be passed.
	 * \param workdir optional workdir to change to before the program inside the child process gets executed.
	 *
	 * \note you may only run one child at a time per process object.
	 */
	void start(const std::string& exe, const params& args,
		const environment& env = environment(), const std::string& workdir = std::string());

	/** sends a terminate signal to the child process. */
	void terminate();

	/** sends a KILL signal to the child process.
	 * \note always try \p terminate() first.
	 */
	void kill();

	/** tests wether the child process has exited already. */
	bool expired();

	/** retrieves the return status code of the child program, if exited, an undefined value otherwise. */
	int result();

private:
	/** setup routine to be invoked from within the child process, to setup the child environment and exec'uting the child program. */
	void setup_child(const std::string& exe, const params& args, const environment& env, const std::string& workdir);

	/** setup routine to be invoked right after the fork() within the parent process. */
	void setup_parent();

	/** fetches the process status, see system's waitpid() for more info. */
	int fetch_status();

private:
	local_stream input_;		//!< redirected stdin stream
	local_stream output_;		//!< redirected stdout stream
	local_stream error_;		//!< redirected stderr stream
	mutable int pid_;			//!< holds the child's process ID
	mutable int status_;		//!< holds the child's process status, see system's waitpid() for more info.
};

// {{{ inlines
inline local_stream::socket& process::input()
{
	return input_.local();
}

inline local_stream::socket& process::output()
{
	return output_.local();
}

inline local_stream::socket& process::error()
{
	return error_.local();
}
// }}}

//@}

} // namespace x0

#endif
