/* <x0/Process.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_process_h
#define sw_x0_process_h 1

#include <x0/LocalStream.h>
#include <x0/Api.h>

#include <vector>
#include <map>
#include <string>

#include <ev++.h>

namespace x0 {

//! \addtogroup base
//@{

/** Creates, runs, and manages a child process running external programs.
 *
 * \note You may only run one child at a time per \p process <b>instance</b>.
 */
class X0_API Process
{
	Process(const Process&) = delete;
	Process& operator=(const Process&) = delete;

public:
	/// vector list used for storing program parameters
	typedef std::vector<std::string> ArgumentList;

	/// string map used for storing custom environment variables.
	typedef std::map<std::string, std::string> Environment;

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
	explicit Process(struct ev_loop *loop);

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
	Process(struct ev_loop *loop, const std::string& exe, const ArgumentList& args,
		const Environment& env = Environment(), const std::string& workdir = std::string());

	~Process();

	int id() const;

	/** socket handle to the STDIN of the child. */
	int input();
	void closeInput();

	/** socket handle to the STDOUT of the child. */
	int output();

	/** socket handle to the STDERR of the child. */
	int error();

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
	int start(const std::string& exe, const ArgumentList& args,
		const Environment& env = Environment(), const std::string& workdir = std::string());

	/** sends a terminate signal to the child process.
	 *
	 * \retval true signal sent
	 * \retval false error sending TERM signal. See errno for diagnostics.
	 *
	 * \see kill()
	 */
	bool terminate();

	/** sends a KILL signal to the child process.
	 *
	 * \retval true signal sent
	 * \retval false error sending TERM signal. See errno for diagnostics.
	 *
	 * \note always try \p terminate() first.
	 */
	bool kill();

	void setStatus(int status);

	/** tests wether the child process has exited already. */
	bool expired();

	/** retrieves the return status code of the child program, if exited, an undefined value otherwise. */
	int result();

	static void dumpCore();

private:
	/** setup routine to be invoked from within the child process, to setup the child environment and exec'uting the child program. */
	void setupChild(const std::string& exe, const ArgumentList& args, const Environment& env, const std::string& workdir);

	/** setup routine to be invoked right after the fork() within the parent process. */
	void setupParent();

private:
	struct ev_loop *loop_;
	LocalStream input_;			//!< redirected stdin stream
	LocalStream output_;		//!< redirected stdout stream
	LocalStream error_;			//!< redirected stderr stream
	mutable int pid_;			//!< holds the child's process ID
	mutable int status_;		//!< holds the child's process status, see system's waitpid() for more info.
};

// {{{ inlines
inline int Process::id() const
{
	return pid_;
}

inline int Process::input()
{
	return input_.local();
}

inline void Process::closeInput()
{
	input_.closeLocal();
}

inline int Process::output()
{
	return output_.local();
}

inline int Process::error()
{
	return error_.local();
}
// }}}

//@}

} // namespace x0

#endif
